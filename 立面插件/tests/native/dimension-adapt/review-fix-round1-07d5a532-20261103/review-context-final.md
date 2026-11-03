# N8a fix round1 scoped re-review — final evidence

The sole original P2 is the scope. Read context/original-fulltask-review.md, context/fix-round1-final-report.md, fix-round1-complete.diff, and the full three new files in focused-source. The complete diff contains all six modified/added review files, including the actual frozen build script diff. Source-index.json and its snapshots remain unchanged. The earlier two-file diff and pre-native manifest remain preserved history, not the complete patch.

Offline commands and outcomes: actual-commands-final.json; RED compiled but failed because the old helper accepted normal return with name==expected; GREEN compiled and both topology and rejection suites passed. No recompile was performed while packaging.

Fresh native result: 428e9c88, case DimensionAdaptSourceBindingFocused, BCE_TEST_RUN, same BC0EEB29...07d5 ARX. Native case is closed partial/errors empty/4 passed. The unchanged formal validate_identity accepts healthy identity and rejects a separate output copy with appended bad-source marker, diagnostic reference source SHA mismatch. No-throw, expected-throw and wrong-stage helper controls all pass. Pointers/globals and original hashes are preserved. native-focused-binding.json binds the raw JSON copies, the exact real bad file hash and root closure/retirement. Explicit stop 70 is not normal QUIT.

Only fix-focused native evidence is added; no old 388-JSON/411-file matrix batch is recopied or rerun. Four 1880/03f2 dimension scenes and 3153 declared roundtrip fields remain independent earlier evidence. The old 15-valid-control claim remains withdrawn: 14 valid old controls plus the new formal-entry source rejection, not a claim that the old 03f2 source control was valid. Glyph/production/current-drawing qualifications remain false.

Implementation does not mark reviewer gate complete; scoped decision belongs to the original reviewer dispatched by root. No source edits, Git, CAD control, or reviewer delegation occurred during this evidence packaging turn.
