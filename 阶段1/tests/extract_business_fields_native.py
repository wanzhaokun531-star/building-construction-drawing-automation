"""Extract a fixed native LIST observation with source lines and closed failures.

This is an evidence parser, not a Tianzheng private-data reader or a drawing
generator. It never assigns opening subtypes/prefixes or converts P to model units.
"""
import argparse
import contextlib
import hashlib
import io
import json
import math
import re
from decimal import Decimal, InvalidOperation
from pathlib import Path


MANIFEST = {
    "CD83": "TDbWall", "CD84": "TDbOpening", "CD89": "TDbOpening",
    "CD94": "TDbGroupOpening", "CF5F": "TDbRectCornerStair",
    "CE8F": "TDbSymbElevation", "CDC0": "TDbText", "CFCE": "TDbDrawingName",
}
MARKER = re.compile(r"^\[S1BUSINESSLIST_(BEGIN|END) handle=([0-9A-F]+) type=([A-Za-z0-9_]+)\]$")
FIELD = re.compile(r"^\s*([^:：]+?)\s*[:：]\s*(.*?)\s*$")
NUMBER = re.compile(r"^([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)(P?)$")
NUMERIC = set("比例 左厚度 右厚度 长度 高度 底标高 左壁面积 右壁面积 洞口粉刷面积 体积 宽度 离地高度 转角 洞墙面积比 踏步高 踏步宽 楼梯高 一跑梯段宽 二跑梯段宽 三跑梯段宽 一跑平台宽度 一跑平台长度 二跑平台宽度 二跑平台长度 扶手宽 扶手边距 起步端扶手伸出距离 终步端扶手伸出距离 文字高度 旋转角度 图名高度 比例高度".split())
INTEGERS = set("一跑梯段步数 二跑梯段步数 三跑梯段步数".split())
PAPER = {"文字高度", "图名高度", "比例高度"}
COMMON = {"DXF名称", "对象句柄", "图层", "比例"}
REQUIRED = {
    "TDbWall": set("对象类型 内外特征 左厚度 右厚度 长度 高度 底标高 左壁面积 右壁面积 洞口粉刷面积 体积 材料".split()),
    "TDbOpening": set("门窗类型 门窗编号 宽度 高度 离地高度 转角 洞墙面积比".split()),
    "TDbGroupOpening": set("门窗类型 门窗编号 宽度 高度 离地高度 转角 洞墙面积比".split()),
    "TDbRectCornerStair": set("对象类型 一跑梯段步数 二跑梯段步数 三跑梯段步数 踏步高 踏步宽 楼梯高 一跑梯段宽 二跑梯段宽 三跑梯段宽 一跑平台宽度 一跑平台长度 二跑平台宽度 二跑平台长度 扶手宽 扶手边距 起步端扶手伸出距离 终步端扶手伸出距离 底标高 楼层标记 上楼方向".split()),
    "TDbSymbElevation": set("对象类型 文字内容 文字样式 文字高度 引线 基线".split()),
    "TDbText": set("对象类型 文字内容 文字样式 对正方式 文字高度 旋转角度".split()),
    "TDbDrawingName": set("对象类型 图名内容 图名样式 图名高度 比例内容 比例样式 比例高度".split()),
}
SEMANTICS = {
    "宽度": "width", "高度": "height", "门窗类型": "opening_kind_text", "门窗编号": "opening_number_text",
    "离地高度": "sill_or_base_offset", "左厚度": "left_thickness", "右厚度": "right_thickness",
    "文字内容": "text_content", "文字高度": "text_height", "图名内容": "drawing_name_text",
    "比例内容": "drawing_scale_text", "比例": "entity_scale", "内外特征": "inside_outside_text",
    "对象类型": "object_kind_text", "图名高度": "drawing_name_height", "比例高度": "drawing_scale_text_height",
}


class ValueProblem(ValueError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def parsed_value(label, raw):
    if label not in NUMERIC | INTEGERS:
        return {"kind": "text", "value": raw, "unit": None}, ("text", raw)
    token = raw.strip()
    if token.lower().rstrip("p") in {"nan", "+nan", "-nan", "inf", "+inf", "-inf", "infinity", "+infinity", "-infinity"}:
        raise ValueProblem("NON_FINITE_NUMBER", "non-finite number is not usable as a native double")
    match = NUMBER.fullmatch(token)
    if not match:
        raise ValueProblem("INVALID_NUMBER", "numeric field has no complete supported numeric token")
    literal, unit = match.groups()
    try:
        decimal = Decimal(literal)
        finite = decimal.is_finite() and math.isfinite(float(decimal))
    except (InvalidOperation, OverflowError):
        finite = False
    if not finite:
        raise ValueProblem("NON_FINITE_NUMBER", "number is not finite in the native double range")
    if unit and label not in PAPER:
        raise ValueProblem("UNEXPECTED_PAPER_UNIT", "P is not an observed unit for this model-parameter field")
    if label in INTEGERS and decimal != decimal.to_integral_value():
        raise ValueProblem("INVALID_INTEGER", "step count must be an integer")
    value = int(decimal) if decimal == decimal.to_integral_value() else float(decimal)
    kind = "integer" if label in INTEGERS else "number"
    return {"kind": kind, "value": value, "decimal": format(decimal, "f"), "unit": unit or None}, (kind, decimal, unit or None)


def add_error(report, code, message, **source):
    report["errors"].append({"code": code, "message": message, **source})


def final_status(report):
    report["status"] = "FAIL" if report["errors"] else "PASS"
    report["usableAsReadEvidence"] = not report["errors"]
    report["automationReady"] = False
    return report


def extract_text(text, source="native LIST text", geometry=None):
    report = {"contract": "stage1.business-fields-extraction.v1", "source": source,
              "entities": [], "checks": [], "errors": [], "automationReady": False,
              "scope": "eight fixed native LIST specimens; source-formatted read evidence only"}
    sections, current = [], None
    for line_number, raw in enumerate(text.splitlines(), 1):
        token = raw.strip()
        match = MARKER.fullmatch(token)
        if match:
            action, handle, kind = match.groups()
            if action == "BEGIN":
                if current is not None:
                    add_error(report, "MISSING_END", "new section started before the previous end marker", handle=current["handle"], line=line_number)
                current = {"handle": handle, "type": kind, "beginLine": line_number, "lines": []}
            elif current is None:
                add_error(report, "UNEXPECTED_END", "end marker has no open section", handle=handle, line=line_number)
            elif (handle, kind) != (current["handle"], current["type"]):
                add_error(report, "MISMATCHED_END", "end marker identity differs from its open section", handle=handle, line=line_number)
                current = None
            else:
                current["endLine"] = line_number
                sections.append(current)
                current = None
        elif "[S1BUSINESSLIST_" in token:
            add_error(report, "MALFORMED_MARKER", "marker is not a complete supported identity marker", line=line_number)
        elif current is not None and token:
            current["lines"].append((line_number, raw))
    if current is not None:
        add_error(report, "MISSING_END", "last section has no end marker", handle=current["handle"], line=current["beginLine"])
    handles = [x["handle"] for x in sections]
    for handle, kind in MANIFEST.items():
        count = handles.count(handle)
        if count != 1:
            add_error(report, "MISSING_SECTION" if count == 0 else "DUPLICATE_SECTION", "each fixed entity must have exactly one complete section", handle=handle, count=count)
    if handles != list(MANIFEST):
        add_error(report, "SECTION_ORDER", "complete sections do not match the fixed eight-entity order")
    for section in sections:
        handle, kind = section["handle"], section["type"]
        if MANIFEST.get(handle) != kind:
            add_error(report, "UNEXPECTED_IDENTITY", "native marker identity differs from the fixed manifest", handle=handle, type=kind)
        entity = {"handle": handle, "nativeType": kind, "beginLine": section["beginLine"], "endLine": section["endLine"], "fields": {}}
        grouped = {}
        for line_number, raw in section["lines"]:
            match = FIELD.fullmatch(raw)
            if not match:
                add_error(report, "UNPARSED_LINE", "nonempty object text is not a complete label/value line; no continuation inferred", handle=handle, line=line_number, raw=raw)
                continue
            label, raw_value = match.groups()
            label = label.strip()
            occurrence = {"source": source, "line": line_number, "rawLine": raw, "rawValue": raw_value}
            try:
                parsed, key = parsed_value(label, raw_value)
                occurrence["parsed"] = parsed
            except ValueProblem as problem:
                parsed, key = None, None
                occurrence["parsed"] = None
                add_error(report, problem.code, str(problem), handle=handle, field=label, line=line_number)
            grouped.setdefault(label, []).append((key, occurrence))
        for label, occurrences in grouped.items():
            keys = [key for key, _ in occurrences]
            valid = all(key is not None for key in keys)
            same = valid and all(key == keys[0] for key in keys)
            state = "valid" if same and len(keys) == 1 else "repeated identical" if same else "conflict" if valid else "invalid"
            field = {"label": label, "semanticKey": SEMANTICS.get(label), "status": state,
                     "value": None, "unit": None, "sources": [occurrence for _, occurrence in occurrences]}
            if same:
                field.update(occurrences[0][1]["parsed"])
            elif valid:
                add_error(report, "FIELD_CONFLICT", "different values or units for one field; no candidate selected", handle=handle, field=label, lines=[x[1]["line"] for x in occurrences])
            entity["fields"][label] = field
        required = COMMON | REQUIRED.get(kind, set())
        for label in sorted(required - entity["fields"].keys()):
            add_error(report, "MISSING_FIELD", "required observed-fixture field is absent; blank values must be explicit", handle=handle, field=label)
        identity = entity["fields"].get("对象句柄", {}).get("value")
        if identity is None or str(identity).upper() != handle:
            add_error(report, "HANDLE_MISMATCH", "listed object identity does not match the enclosing marker", handle=handle)
        report["entities"].append(entity)
    add_consistency_checks(report, geometry)
    report["limitations"] = [
        "Native LIST supplies formatted text; arbitrary non-integer internal-double precision is not established.",
        "Bare numeric units are not invented. P remains P, with no entity-scale conversion.",
        "组合门 is retained verbatim; it is not classified as 门连窗/FDM. Child segments and opening methods are not inferred.",
        "窗/门 text does not establish sliding-window subtype, all opening variants, creation or writable parameters.",
        "Elevation text 0 is text; it is not promoted to a verified completion-surface elevation parameter.",
        "Empty opening number and drawing scale content remain explicit empty strings; entity scale 100 does not fill them.",
        "No host relationship, regulatory compliance, visual equivalence or phase-2 readiness follows from extraction.",
    ]
    return final_status(report)


def add_consistency_checks(report, geometry):
    entities = {e["handle"]: e for e in report["entities"]}

    def value(handle, label):
        field = entities.get(handle, {}).get("fields", {}).get(label, {})
        return field.get("value") if field.get("status") in ("valid", "repeated identical") else None

    def check(cid, observed, reference, evidence, method):
        if observed is None or reference is None:
            return
        finite = all(isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x) for x in (observed, reference))
        passed = finite and math.isclose(observed, reference, abs_tol=1e-5, rel_tol=1e-10)
        report["checks"].append({"id": cid, "observed": observed, "reference": reference, "passed": passed, "evidence": evidence, "method": method})
        if not passed:
            add_error(report, "CHECK_MISMATCH", "observed LIST value disagrees with its explicit control", check=cid)

    counts = [value("CF5F", f"{run}跑梯段步数") for run in ("一", "二", "三")]
    rise = value("CF5F", "踏步高")
    if all(isinstance(x, (int, float)) for x in counts + [rise]):
        check("CF5F.stepSumHeight", value("CF5F", "楼梯高"), sum(counts)*rise, "native LIST arithmetic only", "sum of three observed run-step counts times observed riser height; no generic stair/riser convention or compliance inferred")
    if geometry is None:
        return
    native = {e.get("handle"): e for e in geometry.get("originalBefore", [])}
    for handle, kind in MANIFEST.items():
        if native.get(handle, {}).get("type") != kind:
            add_error(report, "GEOMETRY_IDENTITY", "native geometry control is missing or has another class", handle=handle)
    wall = native.get("CD83", {})
    check("CD83.length.curve", value("CD83", "长度"), (wall.get("curve") or {}).get("length"), "native AcDbCurve length", "observed wall length against the exact public native curve control")
    low, high = wall.get("min"), wall.get("max")
    if isinstance(low, list) and isinstance(high, list) and len(low) == len(high) == 3:
        left, right = value("CD83", "左厚度"), value("CD83", "右厚度")
        if left is not None and right is not None:
            check("CD83.thickness.bboxY", left+right, high[1]-low[1], "native wall bounds", "this horizontal wall: left+right thickness against native Y span")
        check("CD83.height.bboxZ", value("CD83", "高度"), high[2]-low[2], "native wall bounds", "this wall: LIST height against native Z span")
    for handle in ("CD84", "CD89", "CD94"):
        grips = native.get(handle, {}).get("grip", {})
        points = grips.get("points", [])
        if grips.get("status") != 0 or not points:
            add_error(report, "MISSING_GEOMETRY", "native opening grip control is unavailable", handle=handle)
            continue
        try:
            width = max(p[0] for p in points)-min(p[0] for p in points)
        except (TypeError, IndexError):
            add_error(report, "INVALID_GEOMETRY", "native grip control is malformed", handle=handle)
            continue
        check(handle+".width.gripXSpan", value(handle, "宽度"), width, "native originalBefore grip coordinates", "fixed horizontal specimen's entire native grip X span; not a generic opening-width, child-segment or host-association algorithm")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def decode_raw(data, ansi_code_page):
    if data.startswith(b"\xef\xbb\xbf"):
        encoding, bom = "utf-8-sig", "UTF-8"
    elif data.startswith(b"\xff\xfe"):
        encoding, bom = "utf-16", "UTF-16LE"
    elif data.startswith(b"\xfe\xff"):
        encoding, bom = "utf-16", "UTF-16BE"
    else:
        encoding, bom = "cp"+str(ansi_code_page), "none"
    return data.decode(encoding, errors="strict"), encoding, bom


def review_text(report, json_path):
    entities = {x["handle"]: x for x in report["entities"]}

    def show(handle, label):
        field = entities.get(handle, {}).get("fields", {}).get(label, {})
        if field.get("status") not in ("valid", "repeated identical"):
            return "[未解决]"
        value = field.get("value")
        return (str(value) if value != "" else '""')+(field.get("unit") or "")

    lines = ["原生 LIST 业务字段解析审查", "", "结论："+report["status"]+"（仅限本次8个样本的只读字段证据，不表示阶段1全量或施工图业务验收通过）。",
             "原始宿主报告及日志未修改；本报告以原始字节日志为准。", "", "输入与工具绑定："]
    for key, val in report.get("inputBinding", {}).items():
        lines.append(str(key)+": "+str(val))
    lines += ["提取JSON: "+str(json_path), "提取JSON SHA256: "+sha256(json_path), "", "实际字段（值后P为日志原样纸面后缀，不换算模型值）："]
    for entity in report["entities"]:
        lines.append(entity["handle"]+" / "+entity["nativeType"]+"，原始日志行 "+str(entity["beginLine"])+".."+str(entity["endLine"]))
        for label, field in entity["fields"].items():
            val = json.dumps(field["value"], ensure_ascii=False, allow_nan=False)
            unit = field.get("unit") or ""
            sources = ",".join(str(s["line"]) for s in field["sources"])
            lines.append("  "+label+" = "+val+unit+"；"+field["status"]+"；来源行 "+sources)
    lines += ["", "核对："]
    for check in report["checks"]:
        lines.append("  "+check["id"]+": "+str(check["observed"])+" vs "+str(check["reference"])+" => "+("PASS" if check["passed"] else "FAIL")+"；"+check["method"])
    lines += ["", "新增读取证据：" if report["status"] == "PASS" else "观测摘要（存在错误，整体禁止作为可用证据）：",
              "  CD83"+show("CD83", "内外特征")+"，左右厚度"+show("CD83", "左厚度")+"/"+show("CD83", "右厚度")+"，长度/高度"+show("CD83", "长度")+"/"+show("CD83", "高度")+"，材料"+show("CD83", "材料")+"。",
              "  CD84"+show("CD84", "门窗类型")+show("CD84", "宽度")+"×"+show("CD84", "高度")+"/离地"+show("CD84", "离地高度")+"；CD89"+show("CD89", "门窗类型")+show("CD89", "宽度")+"×"+show("CD89", "高度")+"；CD94原类型文本"+show("CD94", "门窗类型")+"，"+show("CD94", "宽度")+"×"+show("CD94", "高度")+"。高度为原生LIST字段，不冒充门窗三维几何高度验收。",
              "  CF5F三跑"+"/".join(show("CF5F", x+"跑梯段步数") for x in ("一", "二", "三"))+"步，踏步高/宽"+show("CF5F", "踏步高")+"/"+show("CF5F", "踏步宽")+"，楼梯高"+show("CF5F", "楼梯高")+"；所有重复来源均保留。",
              "  CE8F文字原文"+show("CE8F", "文字内容")+"，CDC0文字"+show("CDC0", "文字内容")+"，CFCE图名"+show("CFCE", "图名内容")+"；文字/图名字高"+show("CDC0", "文字高度")+"/"+show("CFCE", "图名高度")+"，不按比例相乘。",
              "  图名比例内容原文"+show("CFCE", "比例内容")+"；门窗编号和比例内容均未补默认值。组合门不等于门连窗，未生成FDM/LC等前缀。", "", "限制："]
    lines += ["  "+x for x in report["limitations"]]
    lines += ["", "底层原生观测复核（当前只读validator输出，未改写原验收记录）：", report.get("hostValidation", {}).get("output", "未附底层验收输出").strip()]
    lines += ["", "错误："]+["  "+json.dumps(x, ensure_ascii=False) for x in report["errors"]]
    lines += ["  无" if not report["errors"] else "  存在错误：整体禁止作为可用读取证据；冲突字段不选择首值。", "", "反例测试执行记录由操作者按实际结果补充；历史validator-at-run.py/宿主JSON保持不变。"]
    return "\n".join(lines)+"\n"


def extract_evidence(host_path, output_path, review_path):
    host_path, output_path, review_path = (p.resolve() for p in (host_path, output_path, review_path))
    run = host_path.parent
    if output_path.parent != run or review_path.parent != run or output_path.exists() or review_path.exists():
        raise ValueError("new extraction/review files must be absent and inside the source run; no overwrite")
    host_bytes = host_path.read_bytes()
    host = json.loads(host_bytes.decode("utf-8-sig"))
    raw_path = Path(host["logFile"]["path"]).resolve()
    if raw_path.parent != run or not raw_path.is_file():
        raise ValueError("authoritative raw log is not an existing file inside the host-evidence run")
    raw = raw_path.read_bytes()
    text, encoding, bom = decode_raw(raw, host["logFile"]["ansiCodePage"])
    report = extract_text(text, str(raw_path), host)
    if len(raw) != host["logFile"]["bytes"] or bom != host["logFile"]["bom"]:
        add_error(report, "RAW_LOG_BINDING", "raw byte count/BOM does not match the immutable host report")
    import verify_business_list_native
    gate_text = io.StringIO()
    with contextlib.redirect_stdout(gate_text):
        gate_status = verify_business_list_native.main(host_path)
    if gate_status:
        add_error(report, "NATIVE_OBSERVATION_VALIDATION_FAILED", "current read-only verifier rejected the underlying native observation")
    here = Path(__file__).resolve()
    test_path = here.with_name("test_business_fields_extraction.py")
    decoded = run / "LIST输出_解码.txt"
    report["inputBinding"] = {
        "hostEvidence": str(host_path), "hostEvidenceSHA256": hashlib.sha256(host_bytes).hexdigest().upper(),
        "rawLog": str(raw_path), "rawLogSHA256": hashlib.sha256(raw).hexdigest().upper(), "rawLogBytes": len(raw),
        "encoding": encoding, "bom": bom, "ansiCodePage": host["logFile"]["ansiCodePage"],
        "parser": str(here), "parserSHA256": sha256(here), "testsSHA256": sha256(test_path),
        "decodedViewSHA256": sha256(decoded) if decoded.is_file() else None,
        "decodedViewAuthority": "reference only; extraction uses strict decoding of raw bytes",
        "nativeVerifierSHA256": sha256(Path(verify_business_list_native.__file__).resolve()),
    }
    report["hostValidation"] = {"exitCode": gate_status, "output": gate_text.getvalue(), "sourceReportModified": False}
    final_status(report)
    payload = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False)+"\n"
    with output_path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(payload)
    with review_path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(review_text(report, output_path))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host_evidence", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--review", type=Path)
    args = parser.parse_args()
    output = args.output or args.host_evidence.parent / "business-fields-extraction.json"
    review = args.review or args.host_evidence.parent / "business-fields-extraction-review.txt"
    report = extract_evidence(args.host_evidence, output, review)
    print("status="+report["status"]+" entities="+str(len(report["entities"]))+" checks="+str(len(report["checks"]))+" errors="+str(len(report["errors"])))
    print("Scope: observed native LIST field extraction only; no creation/write/association or phase-2 claim.")
    return int(bool(report["errors"]))


if __name__ == "__main__":
    raise SystemExit(main())
