# N9b pure C++ text coordinates and observed occupancy

Build from `立面插件` with `./tools/build_text_occupancy.ps1 -Label run`.
The script copies its inputs before building two isolated consoles with VS2019,
v141 14.16 / x64 / C++17. It does not build or load an ARX.

`TextOccupancyTests.cpp` covers the explicit range convention, single backward
reflection, all four corners, a hand-calculated rotation/shear/net transform,
the exact N9a rotated-backward range and direction, negative minima, invalid
transforms, complete antialiased cells, padded rows, full scanning, inverse
four-corner mapping, blank/edge rejection, byte-length/overflow and context.

Using the bundled development Python, run:

```powershell
& 'C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' `
  ./tools/verify_text_occupancy.py --receipt <build/receipt.json> --output <new-directory>
```

The verifier decodes the ten existing RGB PNGs; there is no rendering or PDF
parsing. It binds the actual PNG, native report, measured mapping, font hashes,
frozen comparator and this build's source/EXE. The compiled adapter returns
the C++ footprint, four world corners, AABB, mapping and context. The unchanged
N9a `full_cells` checks every ink cell independently. It also rejects a 35%
shrink of each result and results computed using incorrect/omitted mappings.
No production `glyphComplete`, `nativeVerified` or qualification is emitted.

RGB layout: three bytes per pixel, R then G then B; positive rowStride;
top-to-bottom rows; opaque white background; any channel below 255 is ink.
The buffer must contain `(height-1)*rowStride+width*3` bytes; final row padding
is not required. Padding and extra trailing bytes are not image pixels.
Pixel `(x,y)` is the full closed cell `[x,x+1] × [y,y+1]`, y down. The phase
identifies that boundary origin in the device frame and is already included in
worldToPixel. Empty or edge-touching observations fail. The result is the device
bounding rectangle of all observed complete cells, inverse-mapped with all four
corners; it can contain white space. It establishes no external scene coverage.

The explicit 2D basis is caller-provided; it must express the intended text
plane direction and normal handedness. Height, xScale, tracking and oblique
must already be represented by the declared range; they are not reapplied.
Only `beforeDisplayBackward` permits one local-X reflection when backward is
true. `afterDisplayBackward` never adds that reflection. Unknown convention,
nonfinite/singular input, invalid range or unrepresentable output fails.
The inherited `Result::canCommit()` reports an available calculation here;
it does not authorize a drawing write or production Layout use.

The unique task report is `执行/N9b-文字坐标与实测占位报告.md`.
`evidence/index.json` points to RED, final GREEN and the real-image checks.
`evidence/baseline.json` is the pre-edit protected-file fingerprint list;
`evidence/new-files.diff` compares each added source with the empty baseline.
No old evidence library or PNG has been copied into this directory.
