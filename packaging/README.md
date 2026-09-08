# Distribution boundary

Phase 0 packages only an explicitly allowlisted source baseline and installs the
small `xem-baseline` executable plus the project license. It is not a playable
release. Arch PKGBUILD, Windows release, and macOS app bundle implementation is
reserved for Phase 13. Editors must be included in those eventual packages.

`python3 tools/repository/source_archive.py` produces a deterministic source
archive from `packaging/source-files.txt`. It rejects symlinks, unsafe paths,
unreviewed extensions, private directories and original-image formats. Review
the explicit file list and run the distribution audit before every release.

Keep `nix/` tooling-only. Use `nix develop path:./nix`, never `path:.`; a Nix path
source copies ignored files unless its input boundary excludes them. A future
Nix build derivation must use an explicit allowlist/fileset of source files and
must pass the same audit. Neither source images nor imported assets are inputs to
the Phase 0 development shell.
