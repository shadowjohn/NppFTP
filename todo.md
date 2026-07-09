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
- [x] Fix FTP PASV endpoint policy: default data connection host to control peer.
- [x] Fix `CUT_StrMethods::RemoveCRLF` unsigned underflow.
- [x] Fix SFTP directory listing path composition overflow.
- [x] Replace default profile password storage with Windows DPAPI or equivalent.
- [x] Pin GitHub Actions by commit SHA and set explicit workflow permissions.
- [x] Add caps for FTP multiline responses and directory listings.
- [x] Fix review follow-ups: isolate release token, make FTP response cap fail closed, use SFTP path helper in production, avoid DPAPI fallback and secret test logs.

## 4. PSPad-style remote browser

- [x] Keep the old tree code until the new current-directory view is usable.
- [x] Add a current path display.
- [x] Add a quick search box for filtering the current directory list.
- [x] Add a change-directory combo box with recent paths and manual entry.
- [x] Show the current directory as a flat list: `..`, folders, files.
- [x] Wire double-click folder navigation and file open/download.
- [ ] Persist only a small recent-directory list per profile.

## 4.1 PSPad-style remote browser polish

- [x] Fix remote browser panel resize/layout after dock shrink, dock expand, and splitter movement.
- [x] Add folder/file icons to the flat list.
- [x] Change list columns to PSPad order: `Name`, `Size`, `Modified`, `Type`, `Permissions`.
- [x] Enable header drag/drop so column order can be changed by the user.
- [x] Populate size, modified date, type, and permissions from `FileObject` metadata.
- [ ] Add click feedback for directory/file activation with a temporary busy cursor.
- [ ] Make `Change dir` Enter navigate known directories and open known files.
- [ ] Make unknown `Change dir` paths no-op without visible error.
- [ ] Align FTP/current path/search/change-dir/list spacing with the PSPad reference screenshots.
- [ ] Keep labels ASCII in source; track localized labels only as a separate resource/i18n task.
- [ ] Run manual Notepad++ QA for resize, icons, metadata columns, header drag, double-click, typed dir/file, and unknown path.

## Not now

- [ ] Do not rewrite the whole FTP window before the flat browser works.
- [ ] Do not add new UI libraries.
- [ ] Do not self-host third-party source archives unless reproducible/offline release builds need it.
