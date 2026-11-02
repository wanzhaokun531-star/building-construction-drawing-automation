"""Build only the stage-one read-only inspection ARX; never loads AutoCAD."""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
PHASE = ROOT / '立面插件/阶段1'
SDK = Path('C:/Autodesk/ObjectARX_for_AutoCAD_2020_Win_64_bit')
VCVARS = Path('C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvars64.bat')
SOURCE = Path(__file__).with_name('ReferenceInspect.cpp')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main():
    assert SOURCE.is_file() and VCVARS.is_file()
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    run = PHASE / 'build' / stamp
    run.mkdir(parents=True)
    source_copy = run / SOURCE.name
    source_copy.write_bytes(SOURCE.read_bytes())
    inputs = [SOURCE] + sorted(SOURCE.parent.glob('*.h'))
    policy_test = SOURCE.with_name('ReferenceCapturePolicyTests.cpp')
    if policy_test.is_file():
        inputs.append(policy_test)
    for path in inputs[1:]:
        (run / path.name).write_bytes(path.read_bytes())
    binary = run / 'ReferenceInspect.arx'
    lines = [
        '@echo off',
        f'call "{VCVARS}" -vcvars_ver=14.16 >nul',
        'if errorlevel 1 exit /b %errorlevel%',
        'cl.exe /nologo /std:c++17 /EHsc /MD /O2 /W4 /utf-8 '
        '/DWIN32 /D_WINDOWS /D_USRDLL /DUNICODE /D_UNICODE /DNOMINMAX '
        f'/I"{SDK / "inc"}" /I"{SDK / "inc-x64"}" '
        '/Fo"ReferenceInspect.obj" "ReferenceInspect.cpp" '
        '/link /DLL /OUT:"ReferenceInspect.arx" /MACHINE:X64 /SUBSYSTEM:WINDOWS '
        f'/INCREMENTAL:NO /DEF:"{SDK / "inc/AcRxDefault.def"}" '
        f'/LIBPATH:"{SDK / "lib-x64"}" '
        'rxapi.lib acdb23.lib acge23.lib accore.lib ac1st23.lib user32.lib advapi32.lib',
    ]
    if policy_test.is_file():
        lines.extend([
            'if errorlevel 1 exit /b %errorlevel%',
            'cl.exe /nologo /std:c++17 /EHsc /MD /O2 /W4 /utf-8 '
            '/Fo"ReferenceCapturePolicyTests.obj" /Fe"ReferenceCapturePolicyTests.exe" '
            '"ReferenceCapturePolicyTests.cpp" /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO',
            'if errorlevel 1 exit /b %errorlevel%',
            'ReferenceCapturePolicyTests.exe',
        ])
    lines.append('exit /b %errorlevel%')
    command = run / 'build.cmd'
    command.write_text('\n'.join(lines) + '\n', encoding='mbcs', newline='\r\n')
    log = run / 'build.log'
    with log.open('wb') as output:
        result = subprocess.run(['cmd.exe', '/d', '/c', str(command)], stdout=output, stderr=subprocess.STDOUT, cwd=run)
    receipt = dict(contract='bce.stage1.readonly-build.v1', source=str(SOURCE), sourceSHA256=sha(source_copy),
                   compiledSource=str(source_copy), configuration='Release/x64/v1411916/C++17',
                   sourceFiles={str(path): sha(run / path.name) for path in inputs},
                   policyTestsIncluded=policy_test.is_file(),
                   SDK=str(SDK), exitCode=result.returncode, binary=str(binary),
                   binarySHA256=sha(binary) if binary.is_file() else None, log=str(log))
    (run / 'build-receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(receipt, ensure_ascii=False))
    if result.returncode:
        print(log.read_text(encoding='utf-8', errors='replace')[-10000:])
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
