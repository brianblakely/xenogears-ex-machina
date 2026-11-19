# rcheevos (vendored)

RetroAchievements' client library, [rcheevos](https://github.com/RetroAchievements/rcheevos)
release **v12.5.0** (tag `v12.5.0`, commit `1433173220a7eaede6a9ed7a18e94117be1821e0`),
under the MIT License in `LICENSE` (Copyright (c) 2018 RetroAchievements.org). The
release notices (`tools/release/notices.py`) carry that licence.

The files are upstream's, unchanged, at the paths the release has them: the client
(`src/rc_client.c`), the server API (`src/rapi`), the achievement runtime
(`src/rcheevos`), its utilities (`src/rc_compat.c`, `src/rc_util.c`,
`src/rc_version.c`) and the disc hashing a PlayStation disc needs (`src/rhash`:
`hash.c`, `hash_disc.c`, `cdreader.c`, `md5.c`), with the public headers they
include. Left out: RAIntegration and the external client (Windows' toolkit and its
bridge), libretro's helpers, the editor API, and the cartridge, archive and
encrypted image hashes (`build.rs` defines `RC_HASH_NO_ROM`, `RC_HASH_NO_ZIP` and
`RC_HASH_NO_ENCRYPTED`), which no PlayStation disc uses.

To update: copy the same files from the new release's tag, run
`cargo test -p xem-achievements` (its tests check the structures `src/ffi.rs`
mirrors against the C compiler's layouts, and the client against a stand-in
server), and update the version here, in `src/ffi.rs` and in docs/runtime.md.
