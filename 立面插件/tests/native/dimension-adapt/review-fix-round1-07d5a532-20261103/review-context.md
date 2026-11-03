# N8a fix round1 scoped review package

Scope: review the original sole P2 and all code that implements its fix. This is not another full N8a task review. No task commit exists; the complete fix diff is generated from exact frozen 1880 and 07d5 build inputs.

Read original-fulltask-review.md, fix-round1-complete.diff, and the full added RejectionAcceptance.h, RejectionAcceptanceTests.cpp, RejectionFocus.inc under focused-source. The actual build_dimension_adapt.ps1 difference is included in the complete diff, and both frozen script versions are packaged. The earlier two-file scoped diff and focused manifest remain byte-preserved under preserved-prior-artifacts; neither is the complete patch.

Changed review files: DimensionAdaptDriver.cpp, DimensionAdaptGreen.inc, tools/build_dimension_adapt.ps1, and three added rejection files. There are six changed/new review files. All eleven 07d5 compiled/source-tool inputs and eight 1880 inputs are copied with SHA bindings; unchanged topology, project and NativeEvidence files are included as context. core-preservation evidence verifies adaptation/resource/roundtrip/finally sections unchanged.

Offline RED really failed after compilation because the old helper accepted a normal return with name==expected. GREEN passed the original topology suite plus normal-return, expected-exception and wrong-stage rejection controls. Formal source-SHA native control calls unchanged NativeEvidence.validate_identity first with a healthy Config, then with a Config clone referencing an independently written bad output copy. No original source, frozen NativeEvidence, shared host/bridge or unsupported scale code was changed.

The original 03f2 four dimensions / 3153 fields are retained independent evidence; no full matrix rerun is claimed for 07d5. The old claim of 15 valid controls was withdrawn: 14 old controls plus one formal SHA gap pending fresh native evidence. Native result and closure are currently pending and will be bound separately. Do not infer native success from the offline build.

Actual commands and their recorded outcomes are in actual-commands.json and evidence logs. No CAD was launched or controlled, no compilation, Git operation, or source edit was performed while assembling this package. Reviewer dispatch belongs to root.
