# Remote Browser Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Polish the PSPad-style remote browser so it resizes correctly, shows file metadata with icons, gives click feedback, and handles typed paths safely.

**Architecture:** Keep `FTPWindow` as the integration point and keep the old tree/FileObject data flow. Add only small helpers where behavior can be tested without Notepad++.

**Tech Stack:** C++ Win32 common controls, existing `FileObject`, existing `FTPSession`, assert-based test executables, `build.bat`.

## Global Constraints

- Do not rewrite FTP/SFTP queue or wrapper code for UI polish.
- Do not add UI libraries.
- Keep old tree code available until the flat browser fully replaces it.
- Do not change existing file encodings; use patch-sized edits.
- Update `todo.md` and `history.md` after each committed task.

---

### Task 1: Fix Remote Panel Resize

**Files:**
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `src/Windows/FTPWindow.h`

**Interfaces:**
- Consumes: `FTPWindow::LayoutRemoteBrowser()`, `WindowSplitter::OnSize(...)`
- Produces: remote browser controls that refill the tree panel after docking resize or splitter changes

- [ ] Ensure `LayoutRemoteBrowser()` is called after every splitter/layout update that can move the hidden tree panel.
- [ ] Clamp width and height before `MoveWindow()` so shrinking cannot leave negative sizes.
- [ ] Resize the list column widths after every layout pass.
- [ ] Verify manually in Notepad++: shrink dock, enlarge dock, move splitter, reconnect.
- [ ] Run `.\build.bat`.
- [ ] Commit: `Fix flat browser resize layout`.

### Task 2: Add Icons and PSPad-Ordered Columns

**Files:**
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `src/Windows/FTPWindow.h`
- Modify: `src/remote_browser_utils.h`
- Modify: `tests/remote_browser_utils.cpp`

**Interfaces:**
- Consumes: `FileObject::isDir()`, `FileObject::isLink()`, `FileObject::GetSize()`, `FileObject::GetMTime()`, `FileObject::GetMod()`
- Produces: list columns in order `Name`, `Size`, `Modified`, `Type`, `Permissions`

- [ ] Add helper tests for `remote_browser_type_text()`, `remote_browser_format_size()`, and `remote_browser_permission_text()`.
- [ ] Add list columns in PSPad order: `Name`, `Size`, `Modified`, `Type`, `Permissions`.
- [ ] Enable `LVS_EX_HEADERDRAGDROP` so users can drag column order.
- [ ] Add small folder/file icons using native Win32 image list or existing bundled icons; do not add dependencies.
- [ ] Populate each row subitem from `FileObject`.
- [ ] Run `_build\tests\remote_browser_utils.exe`.
- [ ] Run `.\build.bat`.
- [ ] Commit: `Add flat browser metadata columns`.

### Task 3: Cursor Feedback for Remote Actions

**Files:**
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `src/Windows/FTPWindow.h`

**Interfaces:**
- Consumes: `ActivateRemoteListSelection()`, `NavigateRemotePath(...)`, `OnEvent(...)`, `OnDirectoryRefresh(...)`
- Produces: wait cursor while directory navigation/download action is pending

- [ ] Add one `bool m_remoteBusyCursor` member.
- [ ] Set it before queued directory navigation or download-open actions.
- [ ] Clear it when the matching directory load/download completes or fails.
- [ ] Handle `WM_SETCURSOR` over remote controls to show `IDC_WAIT` while the flag is set.
- [ ] Run `.\build.bat`.
- [ ] Commit: `Show busy cursor for flat browser actions`.

### Task 4: Make Change Dir Enter Open Known Files Too

**Files:**
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `src/remote_browser_utils.h`
- Modify: `tests/remote_browser_utils.cpp`

**Interfaces:**
- Consumes: `NavigateRemotePathFromCombo()`, `NavigateRemotePath(...)`, `FTPSession::FindPathObject(...)`
- Produces: Enter on combo navigates to known dirs, opens known files, no-ops for unknown paths

- [ ] Add helper tests for `remote_browser_simplify_typed_path()` with `/abs/path`, `child`, `../sibling`, and overlong input.
- [ ] On Enter, simplify path against current directory.
- [ ] If the target is a known dir, call `SetRemoteCurrentDir(target, true)`.
- [ ] If the target is a known file, call `DownloadFileCache(target->GetPath())`.
- [ ] If the target is unknown, do nothing visible: no beep, no queued failing lookup.
- [ ] Run `_build\tests\remote_browser_utils.exe`.
- [ ] Run `.\build.bat`.
- [ ] Commit: `Handle typed flat browser paths`.

### Task 5: Align Labels Without Encoding Churn

**Files:**
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `history.md`

**Interfaces:**
- Consumes: existing ASCII source encoding
- Produces: PSPad-like control order and spacing without converting source encoding

- [ ] Keep source strings ASCII in this task.
- [ ] Align label order and spacing to the PSPad screenshot: FTP line, current path, quick search, change dir, list.
- [ ] Record localized labels as a separate resource/i18n task instead of mixing encoding work into this patch.
- [ ] Run `.\build.bat`.
- [ ] Commit: `Align flat browser labels`.

### Task 6: Manual QA Pass and Follow-up Triage

**Files:**
- Modify: `todo.md`
- Modify: `history.md`

**Interfaces:**
- Consumes: Tasks 1-5
- Produces: a tested next slice and a short remaining backlog

- [ ] Install `_build\Release\NppFTP.dll` into Notepad++ test profile.
- [ ] Verify resize, icons, metadata columns, header drag, double-click dir/file, typed dir/file, unknown path no-op.
- [ ] Record failures as new unchecked `todo.md` items.
- [ ] Record verification and zip SHA256 in `history.md`.
- [ ] Run `git diff --check`.
- [ ] Commit and push the completed slice.
