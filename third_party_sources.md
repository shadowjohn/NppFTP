# Third-party Sources

Checked: 2026-07-09

Policy:

- Build-time downloads stay URL + SHA256 verified by `build_3rdparty.py`.
- Do not commit upstream tarballs to git.
- Mirror only if offline or reproducible release builds need it, or if an upstream archive becomes unstable.
- For vendored code, keep a digest of the current repo copy until a clean upstream archive and license file are recovered.

Subtree digest recipe:

1. Run `git ls-files <dir> | sort`.
2. SHA256 each listed file.
3. Write `sha256  path` lines with LF endings.
4. SHA256 that manifest text.

| Component | How used | Source | Recorded hash | License | Checked | Fallback / notes |
| --- | --- | --- | --- | --- | --- | --- |
| OpenSSL 4.0.1 | Build-time download via `build_3rdparty.py` | https://github.com/openssl/openssl/releases/download/openssl-4.0.1/openssl-4.0.1.tar.gz | SHA256 `2db3f3a0d6ea4b59e1f094ace2c8cd536dffb87cdc39084c5afa1e6f7f37dd09` | Apache-2.0 | URL HEAD 200 on 2026-07-09 | Official source/checksum page: https://openssl-library.org/source/. Mirror only if the official release artifact disappears. |
| zlib 1.3.2 | Build-time download via `build_3rdparty.py` | https://github.com/madler/zlib/archive/refs/tags/v1.3.2.tar.gz | SHA256 `b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1` | zlib | Official `madler/zlib` tag `v1.3.2` resolved to `216c70c020aa53f0c40920d155f808b6b59c9acb` on 2026-07-20 | Replaces zlib.net fossils delivery after GitHub Actions twice received content that failed the official fossil hash check. The fixed GitHub tag archive remains SHA256-verified before extraction. |
| libssh 0.12.0 | Build-time download via `build_3rdparty.py` | https://www.libssh.org/files/0.12/libssh-0.12.0.tar.xz | SHA256 `1a6af424d8327e5eedef4e5fe7f5b924226dd617ac9f3de80f217d82a36a7121` | LGPL-2.1-or-later | URL HEAD 200 on 2026-07-09 | Same directory publishes detached signatures. Release note: https://www.libssh.org/2026/02/10/libssh-0-12-0-and-0-11-4-security-releases/. |
| TinyXML 2.6.2-derived | Vendored under `tinyxml/` | https://sourceforge.net/projects/tinyxml/files/tinyxml/2.6.2/ | Vendored subtree manifest SHA256 `1b5f254165f1563743fd5b94305cb5102d173c23982d19211edf3c51230134a8` | zlib | Source page HEAD 200 on 2026-07-09 | Repo says this copy is modified from TinyXML 2.6.2. Keep the local digest; recover the original archive hash before replacing the vendored copy. |
| Ultimate TCP/IP v4.2 / UTCP | Vendored under `UTCP/` | Original: https://www.codeproject.com/Articles/20181/The-Ultimate-TCP-IP-Home-Page; mirror: https://sourceforge.net/projects/mfc-ultimate-toolset/files/UltimateTCP-IP42_src.zip/download | Vendored subtree manifest SHA256 `055b26873422fa0dc3c26e8fb05011f6301c20b92e114c0e0aff68576f30af3a` | CodeProject / Ultimate TCP-IP agreement; exact license file is not in this repo | Original page HEAD check failed; SourceForge mirror HEAD 200 on 2026-07-09 | Treat as fragile. Do not create a new public mirror until the matching license file/source package is recovered and stored. |

## Build Tool Bootstrap

| Component | How used | Source | Recorded hash | License | Checked | Fallback / notes |
| --- | --- | --- | --- | --- | --- | --- |
| Strawberry Perl 5.42.2.1 portable | Optional local build tool used by `build_scripts.ps1` when no complete Perl is installed | https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54221_64bit/strawberry-perl-5.42.2.1-64bit-portable.zip | SHA256 `32d83be90cf04b807cfb9477482bc36302cdee6f5b04cf57e81adecbd8f07898` | Perl Artistic License or GPL terms; bundled packages have their own licenses | URL/hash from https://strawberryperl.com/releases.json on 2026-07-09 | Downloaded only into ignored `_build/tools`; not committed or installed system-wide. |
