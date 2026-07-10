# Flat Remote File Actions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add safe English flat-list actions: context menus, F2 rename, multi-file upload, drag targets, checkbox CHMOD, and useful single-operation errors.

**Architecture:** FTPWindow remains the UI coordinator and FTPSession remains the only transfer gateway. Add two small native dialogs and header-only conversion helpers; direct-file conflicts use the selected target directory's current FileObject children. QueueOperation reads a normalized failure kind from the current wrapper.

**Tech Stack:** C++11, Win32 common controls/dialogs, existing Dialog base class, FTPSession, QueueOperation, CMake globbed sources, assert-based tests.

## Global Constraints

- New UI copy is English ASCII only.
- Localization, including Traditional Chinese as the future default, is outside this plan.
- No UI library, second queue, persistent overwrite setting, recursive delete, sync, or resume.
- Direct files use target-directory cache only and do not make a network preflight.
- Overwrite-all is set only after Overwrite and resets when the session disconnects.
- SFTP maps permission and missing-path errors; FTP/FTPS remain generic.
- Preserve existing encoding and use minimal patches.

---

## File Map

- Modify src/remote_browser_utils.h and tests/remote_browser_utils.cpp for pure permission, decision, and failure helpers.
- Create src/Windows/ChmodDialog.h/.cpp and OverwriteDialog.h/.cpp.
- Modify src/Windows/NppFTP.rc and resource.h for native dialog resources.
- Modify src/Windows/Commands.h and FTPWindow.h/.cpp for menus, picker, drag targets, and errors.
- Create src/RemoteFailure.h for the independently testable failure enum.
- Modify src/FTPClientWrapper.h/.cpp, FTPClientWrapperSSH.cpp, QueueOperation.h/.cpp for failure propagation.
- CMakeLists.txt needs no edit because it globs src and src/Windows sources.

### Task 1: Permission Helpers And CHMOD Dialog

**Files:**
- Modify: src/remote_browser_utils.h:11-129
- Modify: tests/remote_browser_utils.cpp:8-46
- Create: src/Windows/ChmodDialog.h
- Create: src/Windows/ChmodDialog.cpp
- Modify: src/Windows/resource.h:120-124
- Modify: src/Windows/NppFTP.rc:192-214

**Interfaces:**
- Produces remote_browser_permission_to_octal(const char*, TCHAR*, size_t), remote_browser_octal_to_checks(const TCHAR*, bool[9]), remote_browser_checks_to_octal(const bool[9], TCHAR*, size_t).
- Produces ChmodDialog::Create(HWND, const char*) and ChmodDialog::GetMode().

- [ ] **Step 1: Write the failing helper test**

Append to tests/remote_browser_utils.cpp:

~~~
TCHAR mode[4]{};
bool checks[9]{};
assert(remote_browser_permission_to_octal("drwxr-xr-x", mode, 4) == 0);
assert(_tcscmp(mode, TEXT("755")) == 0);
assert(remote_browser_permission_to_octal("-rw-r-----", mode, 4) == 0);
assert(_tcscmp(mode, TEXT("640")) == 0);
assert(remote_browser_permission_to_octal(NULL, mode, 4) == -1);
assert(remote_browser_octal_to_checks(TEXT("755"), checks) == 0);
assert(remote_browser_checks_to_octal(checks, mode, 4) == 0);
assert(_tcscmp(mode, TEXT("755")) == 0);
~~~

- [ ] **Step 2: Run the test to verify it fails**

Run:

~~~
cmd.exe /C "call \"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat\" -arch=x64 && cl /nologo /EHsc /DUNICODE /D_UNICODE /D_WIN32 /DWIN32 /I. /I.\src /I.\src\Windows /Fo_build\tests\remote_browser_utils.obj /Fe_build\tests\remote_browser_utils.exe tests\remote_browser_utils.cpp shlwapi.lib && _build\tests\remote_browser_utils.exe"
~~~

Expected: compile failure because the helper names do not exist.

- [ ] **Step 3: Implement the helpers and dialog**

Add this helper logic to src/remote_browser_utils.h:

~~~
static inline int remote_browser_permission_to_octal(const char *mod, TCHAR *mode, size_t modeCount)
{
	if (!mod || !mode || modeCount < 4 || strlen(mod) < 10)
		return -1;
	for (int group = 0; group < 3; group++) {
		int value = 0;
		if (mod[1 + group * 3] == 'r') value |= 4;
		if (mod[2 + group * 3] == 'w') value |= 2;
		if (mod[3 + group * 3] == 'x' || mod[3 + group * 3] == 's' || mod[3 + group * 3] == 't') value |= 1;
		mode[group] = (TCHAR)(TEXT('0') + value);
	}
	mode[3] = 0;
	return 0;
}
~~~

Implement the checkbox helpers with the same 4/2/1 bit order and reject invalid octal input.

Create ChmodDialog as a Dialog subclass with Create(HWND, const char*) and GetMode(). Add IDD_DIALOG_CHMOD with Owner, Group, and Other group boxes, nine AUTOCHECKBOX controls, an octal LTEXT, and existing OK/Cancel buttons. On checkbox change, use remote_browser_checks_to_octal; on OK, return result 1 only for a valid three-digit mode.

- [ ] **Step 4: Run test and build**

Run the test command from Step 2, then:

~~~
.\build.bat -Arch x64 -Config Release
~~~

Expected: remote_browser_utils_exit=0 and _build\Release\NppFTP.dll exists.

- [ ] **Step 5: Commit**

~~~
git add src/remote_browser_utils.h tests/remote_browser_utils.cpp src/Windows/ChmodDialog.h src/Windows/ChmodDialog.cpp src/Windows/resource.h src/Windows/NppFTP.rc
git commit -m "feat: add graphical chmod dialog"
~~~

### Task 2: Overwrite Dialog, Context Menus, And Direct Upload

**Files:**
- Create: src/Windows/OverwriteDialog.h
- Create: src/Windows/OverwriteDialog.cpp
- Modify: src/Windows/NppFTP.rc and src/Windows/resource.h
- Modify: src/Windows/Commands.h:27-57
- Modify: src/Windows/FTPWindow.h:82-175
- Modify: src/Windows/FTPWindow.cpp:780-1450, 1537-1714, 2073-2290

**Interfaces:**
- Produces OverwriteDialog::Create(HWND, const TCHAR*), GetDecision(), GetOverwriteAll().
- Produces FTPWindow::ShowRemotePopup, PromptRemoteRename, QueueDirectRemoteUploads, and GetRemoteDropTarget.

- [ ] **Step 1: Write the failing overwrite-decision test**

Add:

~~~
assert(RemoteOverwriteCancel == 0);
assert(RemoteOverwriteOverwrite == 1);
assert(RemoteOverwriteSkip == 2);
~~~

- [ ] **Step 2: Run focused test**

Run the Task 1 test command.

Expected: compile failure until this enum exists in remote_browser_utils.h.

- [ ] **Step 3: Implement the remote list interaction**

Add:

~~~
enum RemoteOverwriteDecision {
	RemoteOverwriteCancel = 0,
	RemoteOverwriteOverwrite = 1,
	RemoteOverwriteSkip = 2
};
~~~

Create IDD_DIALOG_OVERWRITE with filename text, Overwrite, Skip, Cancel, and Don't ask again this session (overwrite all). GetOverwriteAll must be true only when the user chose Overwrite.

Add remote-only command IDs after IDM_POPUP_PASTE. Create and destroy three new menus:
- file: Edit, CHMOD..., Delete file, Rename F2
- directory: Upload files..., CHMOD..., Delete directory, Rename F2
- blank: Refresh, Create directory..., New file..., Upload files...

WM_CONTEXTMENU for m_remoteList must hit-test and select the pointed list item. It uses m_remoteCurrentDir for blank space and returns without a destructive menu for the .. row. LVN_KEYDOWN handles VK_F2 by using InputDialog seeded with FileObject::GetLocalName and calling existing Rename.

Use a local static GetOpenFileName helper in FTPWindow.cpp with OFN_ALLOWMULTISELECT | OFN_EXPLORER, a 32768-TCHAR buffer, and std::vector<std::basic_string<TCHAR>>. Do not expand PathUtils for this single caller.

QueueDirectRemoteUploads(FileObject *target, const std::vector<std::basic_string<TCHAR>>& paths) must:
1. derive the local basename;
2. look for target->GetChildByName(utf8Name);
3. show OverwriteDialog only for known conflicts while overwrite-all is false;
4. return for Cancel, continue for Skip, and set m_overwriteAll only after Overwrite;
5. call m_ftpSession->UploadFile(localPath, target->GetPath(), true, 1).

Add bool m_overwriteAll to FTPWindow, initialize it false in the constructor, and reset it false in OnDisconnect. Remote Edit must call DownloadFileCache on the selected file; remote CHMOD, delete, create, refresh, and rename must call the existing FTPWindow operation for the selected item or m_remoteCurrentDir.

OLE drag hit-testing converts POINTL from screen to m_remoteList client coordinates. Directory item targets itself; file and blank target m_remoteCurrentDir. Queue direct files through QueueDirectRemoteUploads. Record local directories in Output as deferred until the recursive plan is implemented; never pass a directory to UploadFile.

- [ ] **Step 4: Build and manually verify**

Run:

~~~
.\build.bat -Arch x64 -Config Release
~~~

Expected: build succeeds. In Notepad++, verify all three menus, F2, multi-file picker, drop on blank/file/directory, Skip-one-file, and overwrite-all reset after disconnect.

- [ ] **Step 5: Commit**

~~~
git add src/Windows/OverwriteDialog.h src/Windows/OverwriteDialog.cpp src/Windows/NppFTP.rc src/Windows/resource.h src/Windows/Commands.h src/Windows/FTPWindow.h src/Windows/FTPWindow.cpp tests/remote_browser_utils.cpp
git commit -m "feat: add flat remote file actions"
~~~

### Task 3: Single-Operation Failure Feedback

**Files:**
- Create: src/RemoteFailure.h
- Modify: src/FTPClientWrapper.h:25-128
- Modify: src/FTPClientWrapper.cpp:20-78
- Modify: src/FTPClientWrapperSSH.cpp:66-380
- Modify: src/QueueOperation.h:42-90
- Modify: src/QueueOperation.cpp:57-75
- Modify: src/Windows/FTPWindow.h:82-108
- Modify: src/Windows/FTPWindow.cpp:1838-2008

**Interfaces:**
- Produces RemoteFailureKind, FTPClientWrapper::GetFailureKind(), QueueOperation::GetFailureKind(), and FTPWindow::ShowOperationFailure.
- Consumed by the recursive directory upload plan.

- [ ] **Step 1: Write the failing enum test**

Add #include "src/RemoteFailure.h" and then:

~~~
assert(RemoteFailureUnknown == 0);
assert(RemoteFailurePermissionDenied != RemoteFailureNotFound);
~~~

- [ ] **Step 2: Run focused test**

Run the Task 1 test command.

Expected: compile failure because RemoteFailureKind is absent.

- [ ] **Step 3: Propagate the failure kind and show it**

Add this enum to src/RemoteFailure.h and include that header from src/FTPClientWrapper.h:

~~~
enum RemoteFailureKind {
	RemoteFailureUnknown = 0,
	RemoteFailurePermissionDenied,
	RemoteFailureNotFound
};
virtual RemoteFailureKind GetFailureKind() const;
~~~

The base returns Unknown. FTPClientWrapperSSH returns PermissionDenied for SSH_FX_PERMISSION_DENIED, NotFound for SSH_FX_NO_SUCH_FILE, and Unknown otherwise. QueueOperation::GetFailureKind returns m_client ? m_client->GetFailureKind() : RemoteFailureUnknown.

Implement ShowOperationFailure with exactly these messages:
- Permission denied by the remote server.
- The remote file or directory no longer exists.
- The remote server rejected the operation. It may be a permission or path problem.

Call it after OutErr for failed upload, create/remove directory, create/delete file, rename, and chmod. Preserve Output detail and show no modal on success.

- [ ] **Step 4: Verify**

Run the helper test and:

~~~
.\build.bat -Arch x64 -Config Release
~~~

Expected: test prints remote_browser_utils_exit=0 and build succeeds. Manually verify denied and missing SFTP paths are distinct; FTP/FTPS uses only the generic message.

- [ ] **Step 5: Commit and record**

~~~
git add src/RemoteFailure.h src/FTPClientWrapper.h src/FTPClientWrapper.cpp src/FTPClientWrapperSSH.cpp src/QueueOperation.h src/QueueOperation.cpp src/Windows/FTPWindow.h src/Windows/FTPWindow.cpp tests/remote_browser_utils.cpp todo.md history.md
git commit -m "feat: report remote operation failures"
~~~

Tick completed direct-action items in todo.md and append verified command output plus remaining manual QA to history.md.
