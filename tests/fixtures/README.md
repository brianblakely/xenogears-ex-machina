# Fixture boundary

Public tests create small original synthetic fixtures at test time. They prove
tool invariants and bounds handling, not original-game behavior. Original-data
fixtures and execution captures are generated locally under `.local/` against an
exact reference profile and are never uploaded to CI or packaged.

Every future behavioral fixture must name a finding, source profile, checkpoint,
expected result and its provenance. An unknown result cannot become an oracle by
copying the output of the new implementation.
