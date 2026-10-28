# Distribution boundary

`release.yml` builds the playable native and browser application when native
app inputs on master change. Releases begin at v0.5 and advance the patch
version automatically.
Linux uses AppImage, Windows a portable ZIP, macOS an application bundle ZIP,
and Android/Horizon a signed APK. The static web build also deploys to Pages.
Installation and reproduction instructions are in [releases.md](../docs/releases.md).

`python3 tools/repository/source_archive.py` produces a deterministic source
archive from `packaging/source-files.txt`. It rejects symlinks, unsafe paths,
unreviewed extensions, private directories and original-image formats. The one
reviewed binary input is the hash-pinned, independently licensed KEMAR HRTF
measurement data. Review
the explicit file list and run the distribution audit before every release.

Keep `nix/` tooling-only. Use `nix develop path:./nix`, never `path:.`; a Nix path
source copies ignored files unless its input boundary excludes them. A future
Nix build derivation must use an explicit allowlist/fileset of source files and
must pass the same audit. Neither source images nor imported assets are inputs to
the development shell. `linux.nix` receives only staged executables and notices.
The native packages have corresponding-source archives for their bundled
runtime libraries, collected from the actual Nix closure with its pinned build
recipes. `cargo-about`, plus the hash-checked Slint custom terms, generates the
Rust dependency notices. Nothing uploads the user's game data.
