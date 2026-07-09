# TODO

## Done

- [x] Record Codex Security scan summary in `history.md`.
- [x] Confirm current third-party downloads already have URL + SHA256 in `build_3rdparty.py`.

## 1. Third-party source ledger

- [x] Add `third_party_sources.md` with current source URL, SHA256, upstream project, license, and fallback notes for:
  - OpenSSL 4.0.1
  - zlib 1.3.2
  - libssh 0.12.0
  - bundled TinyXML 2.6.2-derived copy
  - bundled UTCP code
- [x] Verify each upstream URL and record the date checked.
- [x] Do not vendor tarballs into git yet.
- [x] Mirror tarballs only if builds must work offline or upstream archives become unstable.

## 2. Baseline build

- [x] Add `build.bat` and `build_scripts.ps1` to check the local build environment and run the selected baseline path.
- [x] Get one local build path working first: MinGW-w64 or Visual Studio, not both.
- [x] Record exact compiler/tool versions in `history.md`.
- [x] Keep dependency downloads hash-checked before any compile.

## 3. Security fixes, risk order

- [x] Fix FTPS hostname verification in `src/FTPClientWrapperSSL.cpp`.
- [x] Scope accepted FTPS certificate exceptions by host/profile, not global DER only.
- [x] Fix cache path traversal: reject `..`, canonicalize final path, enforce cache-root containment.
- [x] Fix FTP PASV parsing overflow: parse octets as bounded integers before formatting.
- [ ] Fix FTP PASV endpoint policy: default data connection host to control peer.
- [ ] Fix `CUT_StrMethods::RemoveCRLF` unsigned underflow.
- [ ] Fix SFTP directory listing path composition overflow.
- [ ] Replace default profile password storage with Windows DPAPI or equivalent.
- [ ] Pin GitHub Actions by commit SHA and set explicit workflow permissions.
- [ ] Add caps for FTP multiline responses and directory listings.

## 4. PSPad-style remote browser

- [ ] Keep the old tree code until the new current-directory view is usable.
- [ ] Add a current path display.
- [ ] Add a quick search box for filtering the current directory list.
- [ ] Add a change-directory combo box with recent paths and manual entry.
- [ ] Show the current directory as a flat list: `..`, folders, files.
- [ ] Wire double-click folder navigation and file open/download.
- [ ] Persist only a small recent-directory list per profile.

## Not now

- [ ] Do not rewrite the whole FTP window before the flat browser works.
- [ ] Do not add new UI libraries.
- [ ] Do not self-host third-party source archives unless reproducible/offline release builds need it.
