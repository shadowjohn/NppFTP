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
- [x] Persist only a small recent-directory list per profile.
- [x] Show prefix-matching recent directories while typing in Change dir.

## 4.1 PSPad-style remote browser polish

- [x] Fix remote browser panel resize/layout after dock shrink, dock expand, and splitter movement.
- [x] Add folder/file icons to the flat list.
- [x] Change list columns to PSPad order: `Name`, `Size`, `Modified`, `Type`, `Permissions`.
- [x] Enable header drag/drop so column order can be changed by the user.
- [x] Populate size, modified date, type, and permissions from `FileObject` metadata.
- [x] Add click feedback for directory/file activation with a temporary busy cursor.
- [x] Make Enter activate the focused flat-list directory or file exactly like double-click.
- [x] Make `Change dir` Enter navigate known directories and open known files.
- [x] Make unknown `Change dir` paths no-op without visible error.
- [x] Align FTP/current path/search/change-dir/list spacing with the PSPad reference screenshots.
- [x] Keep labels ASCII in source; track localized labels only as a separate resource/i18n task.
- [ ] Run manual Notepad++ QA for resize, icons, metadata columns, header drag, double-click/Enter activation, typed dir/file, and unknown path.

## 5. Flat remote file operations

- [x] Add flat-list context menus, keyboard F2 rename, and current-directory blank-area commands.
- [x] Add multi-file picker and drag/drop target routing with session-only overwrite confirmation.
- [x] Add native checkbox-based CHMOD dialog with synchronized octal mode.
- [x] Add an owned recursive-upload planner with bounded paths and no reparse-point traversal.
- [x] Add remote scan and parent-first mkdir/upload queue operations with disconnect-safe batch ownership.
- [x] Add recursive local-directory upload, safe remote-directory merge, and per-file queue progress.
- [x] Show actionable single-operation mutation failures.
- [x] Show one failure summary for recursive uploads.
- [x] Route F2 rename through Notepad++ modeless-dialog key handling.
- [x] Preserve flat-list focus after a successful mutation refresh: focus the renamed item after rename, the same item after CHMOD, and the new file after creation; select it and scroll it into view.
- [ ] Run manual Notepad++ QA for flat-list menus, F2, picker/drop targets, Skip, Cancel, and session overwrite-all reset.
- [ ] Run manual SFTP/FTP QA for permission-denied, missing-path, and generic operation failure messages.
- [ ] Run real-server recursive-upload QA for FTP/SFTP target routing, new/existing directory merge, symlinks, nested collisions, every progress row, and one batch summary.

## 6. Localization

- [ ] Add UI language selection in a separate slice; default to Traditional Chinese.

## 7. User-directed remote download

- [x] Route toolbar and flat-list Download commands through a Save As/folder-picker workflow; keep Edit cache-only.
- [x] Download a selected remote directory recursively into a same-named local root.
- [x] Guard local output paths, skip remote symlinks, and resolve file collisions per batch.
- [x] Show per-file queue progress and one directory-download failure summary.
- [x] Add focused plan tests.
- [ ] Run manual QA: file Save As cancel/success/no-open; folder picker root creation; nested files; overwrite/skip/all/cancel; local file/directory conflicts; scan failure; FTP/FTPS/SFTP queue progress and one summary.

## Not now

- [ ] Do not rewrite the whole FTP window before the flat browser works.
- [ ] Do not add new UI libraries.
- [ ] Do not self-host third-party source archives unless reproducible/offline release builds need it.
