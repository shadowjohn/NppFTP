# Flat Remote List Navigation, Context, and Sorting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Backspace parent navigation, LIST-before-enter failure handling, row-aware create actions, and five-column view-only sorting to the flat remote list.

**Architecture:** Keep `FTPWindow` as the single Win32 integration point and reuse the existing `FTPSession` queues and mutation methods. Route every directory transition through the existing pending-path LIST flow, keep sorting in a local vector of pointer-backed sort keys so `FileObject` children and the legacy tree are untouched, and use small header-only helpers only where behavior needs a focused executable test.

**Tech Stack:** C++17, Win32 ListView/Header controls, existing `FileObject`/`FTPSession`/`FTPQueue`, assert-based MSVC tests, CMake x64 Release build

## Global Constraints

- This plan consumes the completed 64-bit plan: `FileObject::GetSize()` returns `LONGLONG`.
- Preflight is the actual content-loading LIST; do not issue a second permission-check request.
- Treat zero files as success and only `-1` as LIST failure.
- Keep `..` fixed at row 0 and keep directories before files in both sort directions.
- Support Name, Size, Modified, Type, and Permissions; one active column only, first click ascending, repeated click toggles.
- Use Windows user-locale, case-insensitive text comparison with an ordinal path fallback.
- Backspace acts only when the remote ListView has focus; root is a no-op.
- Do not add dependencies, persist sort settings, or alter legacy tree keyboard/context/sort behavior.

## File Map

- `src/remote_browser_utils.h`: keyboard gate, pending-path/result predicates, and create-target predicate.
- `src/RemoteListSort.h`: pure pointer-backed sort key and comparator.
- `src/Windows/FTPWindow.h/.cpp`: ListView events, pending navigation, context commands, sorting state, and header arrows.
- `src/FTPQueue.cpp`: duplicate-operation ownership fix.
- `tests/remote_list_keyboard.cpp`: real Win32 modeless Backspace delivery.
- `tests/remote_browser_utils.cpp`: navigation/result and create-target predicates.
- `tests/remote_list_sort.cpp`: five columns, direction, grouping, locale tie-break, and 64-bit Size.
- `README.md`, `history.md`: shipped behavior and verification record.

---

### Task 1: Backspace and LIST-before-enter Navigation

**Files:**
- Modify: `tests/remote_list_keyboard.cpp`
- Modify: `tests/remote_browser_utils.cpp`
- Modify: `src/remote_browser_utils.h`
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `src/FTPQueue.cpp`

**Interfaces:**
- Consumes: existing `m_remotePendingPath`, `m_remoteBusyCursor`, `NavigateRemotePath()`, `GetDirectory()`, `GetDirectoryHierarchy()`, `GetRemoteFailureMessage()`
- Produces: `remote_browser_pending_path_matches()`, `remote_browser_directory_result_succeeded()`, Backspace delivery, latest-request-wins navigation

- [ ] **Step 1: Add failing keyboard and navigation assertions**

In `tests/remote_list_keyboard.cpp`, add a flag and notification branch:

```cpp
static bool receivedBackspace = false;

if (header->code == LVN_KEYDOWN && ((NMLVKEYDOWN*)lParam)->wVKey == VK_BACK)
	receivedBackspace = true;
```

After the Enter assertions, add:

```cpp
key.wParam = VK_BACK;
assert(IsDialogMessage(parent, &key));
assert(receivedBackspace);
```

In `tests/remote_browser_utils.cpp`, add:

```cpp
assert(remote_browser_directory_result_succeeded(0));
assert(remote_browser_directory_result_succeeded(12));
assert(!remote_browser_directory_result_succeeded(-1));
assert(remote_browser_pending_path_matches("/target", "/target"));
assert(!remote_browser_pending_path_matches("/newest", "/older"));
assert(!remote_browser_pending_path_matches("", "/target"));
assert(!remote_browser_pending_path_matches(NULL, "/target"));
```

- [ ] **Step 2: Compile both tests and verify RED**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$utils = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_browser_utils.cpp /Fe:_build\tests\remote_browser_utils.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $utils
$keys = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_list_keyboard.cpp /Fe:_build\tests\remote_list_keyboard.exe comctl32.lib user32.lib shlwapi.lib'
& cmd.exe /d /s /c $keys
```

Expected: utils compile fails because both predicates are missing; keyboard test fails because Backspace is not requested by `WM_GETDLGCODE`.

- [ ] **Step 3: Add the minimum pure predicates and key gate**

In `src/remote_browser_utils.h`:

```cpp
static inline bool remote_browser_wants_key(const MSG *message)
{
	return message && message->message == WM_KEYDOWN &&
		(message->wParam == VK_F2 || message->wParam == VK_RETURN || message->wParam == VK_BACK);
}

static inline bool remote_browser_directory_result_succeeded(int result)
{
	return result != -1;
}

static inline bool remote_browser_pending_path_matches(const char *pendingPath, const char *completedPath)
{
	return pendingPath && pendingPath[0] && completedPath && strcmp(pendingPath, completedPath) == 0;
}
```

- [ ] **Step 4: Route every directory transition through one pending LIST**

Change directory activation to call the path navigator instead of switching immediately:

```cpp
if (fo->isDir())
	return NavigateRemotePath(fo->GetPath());
```

In `NavigateRemotePathFromCombo()`, retain the existing file-open branch, but route a known directory through:

```cpp
if (targetObj && targetObj->isDir())
	return NavigateRemotePath(targetObj->GetPath());
```

Use this core in `NavigateRemotePath()` after path simplification:

```cpp
FileObject *targetObj = m_ftpSession->FindPathObject(target);
if (targetObj && !targetObj->isDir())
	return -1;

lstrcpynA(m_remotePendingPath, target, MAX_PATH);
m_remoteBusyCursor = true;
int res = targetObj ? m_ftpSession->GetDirectory(target) : m_ftpSession->GetDirectoryHierarchy(target);
if (res != 0) {
	m_remotePendingPath[0] = 0;
	m_remoteBusyCursor = false;
}
return res;
```

Handle Backspace in the existing `LVN_KEYDOWN` branch:

```cpp
if (key->wVKey == VK_BACK) {
	FileObject *parent = m_remoteCurrentDir ? m_remoteCurrentDir->GetParent() : NULL;
	if (parent)
		NavigateRemotePath(parent->GetPath());
	result = TRUE;
	break;
}
```

- [ ] **Step 5: Make directory completion path-aware and failure-safe**

Remove `QueueTypeDirectoryGet` from the generic block that unconditionally clears `m_remoteBusyCursor`. At the top of its detailed end handler, calculate:

```cpp
bool pendingNavigation = remote_browser_pending_path_matches(m_remotePendingPath, dirop->GetDirPath());
```

Replace the current failure fall-through with:

```cpp
if (!remote_browser_directory_result_succeeded(queueResult)) {
	OutErr("[FTPWindow] Failure retrieving contents of directory %T", SU::Utf8ToTChar(dirop->GetDirPath()));
	if (pendingNavigation) {
		m_remotePendingPath[0] = 0;
		m_remoteBusyCursor = false;
		const TCHAR *message = GetRemoteFailureMessage(queueOp->GetFailureKind());
		::MessageBox(m_hwnd, message, TEXT("Unable to enter directory"), MB_OK | MB_ICONERROR);
	}
	break;
}
```

Only after that branch, apply parent listings, write `Loaded directory`, and call `OnDirectoryRefresh()`. After a successful matching completion, ensure the cursor is cleared even though `SetRemoteCurrentDir()` clears the pending path:

```cpp
if (pendingNavigation)
	m_remoteBusyCursor = false;
```

If the final `FindPathObject(dirop->GetDirPath())` still returns `NULL` after applying successful hierarchy parents, treat it as a matching navigation failure: clear pending/cursor, show the same generic enter failure, and do not log `Loaded directory`.

Do not clear pending/cursor or show a modal for stale A when the current pending path is B. A successful stale result may still refresh A's cache.

- [ ] **Step 6: Fix duplicate queue-operation ownership at the shared source**

In `src/FTPQueue.cpp`, include `<memory>` and take ownership at the top of `AddQueueOp()`:

```cpp
int FTPQueue::AddQueueOp(QueueOperation *op)
{
	std::unique_ptr<QueueOperation> owned(op);
	op->SetClient(m_wrapper);
	// existing duplicate scan remains here
```

Leave the duplicate branch as a normal return; `owned` deletes the ignored operation. Transfer ownership only when pushing:

```cpp
m_queue.push_back(owned.release());
```

- [ ] **Step 7: Re-run focused tests and Release build**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$utils = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_browser_utils.cpp /Fe:_build\tests\remote_browser_utils.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $utils
$keys = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_list_keyboard.cpp /Fe:_build\tests\remote_list_keyboard.exe comctl32.lib user32.lib shlwapi.lib'
& cmd.exe /d /s /c $keys
& .\_build\tests\remote_browser_utils.exe
& .\_build\tests\remote_list_keyboard.exe
.\build.bat -Arch x64 -Config Release
```

Expected: both focused tests print exit 0; Release build succeeds. Code inspection confirms the duplicate branch returns while the local `unique_ptr` still owns the ignored operation.

- [ ] **Step 8: Commit Task 1**

```powershell
git add -- tests/remote_list_keyboard.cpp tests/remote_browser_utils.cpp src/remote_browser_utils.h src/Windows/FTPWindow.cpp src/FTPQueue.cpp
git commit -m "feat: preflight remote directory navigation"
```

---

### Task 2: Row-aware Create Directory and New File Actions

**Files:**
- Modify: `tests/remote_browser_utils.cpp`
- Modify: `src/remote_browser_utils.h`
- Modify: `src/Windows/FTPWindow.cpp`

**Interfaces:**
- Consumes: existing `m_currentSelection`, `m_remoteCurrentDir`, `CreateDirectory()`, `CreateFile()`, and remote file/dir/blank menus
- Produces: `remote_browser_create_uses_selected_directory(bool hasSelection, bool isDirectory)`

- [ ] **Step 1: Add failing target-selection assertions**

In `tests/remote_browser_utils.cpp`:

```cpp
assert(remote_browser_create_uses_selected_directory(true, true));
assert(!remote_browser_create_uses_selected_directory(true, false));
assert(!remote_browser_create_uses_selected_directory(false, false));
```

- [ ] **Step 2: Compile and verify RED**

Run Task 1 Step 2's utils command.

Expected: compile failure because `remote_browser_create_uses_selected_directory()` is missing.

- [ ] **Step 3: Add the pure target predicate**

In `src/remote_browser_utils.h`:

```cpp
static inline bool remote_browser_create_uses_selected_directory(bool hasSelection, bool isDirectory)
{
	return hasSelection && isDirectory;
}
```

- [ ] **Step 4: Add create commands to both row menus**

In `CreateMenus()`, append these commands to both row menus, separated from destructive operations:

```cpp
AppendMenu(m_popupRemoteDir, MF_SEPARATOR, 0, NULL);
AppendMenu(m_popupRemoteDir, MF_STRING, IDM_POPUP_REMOTE_NEWDIR, TEXT("Create &directory"));
AppendMenu(m_popupRemoteDir, MF_STRING, IDM_POPUP_REMOTE_NEWFILE, TEXT("&New file"));

AppendMenu(m_popupRemoteFile, MF_SEPARATOR, 0, NULL);
AppendMenu(m_popupRemoteFile, MF_STRING, IDM_POPUP_REMOTE_NEWDIR, TEXT("Create &directory"));
AppendMenu(m_popupRemoteFile, MF_STRING, IDM_POPUP_REMOTE_NEWFILE, TEXT("&New file"));
```

Keep the existing blank menu and the existing `..` no-menu guard unchanged.

- [ ] **Step 5: Resolve the create parent from the right-clicked row**

Use the same target expression in both command handlers:

```cpp
bool useSelection = remote_browser_create_uses_selected_directory(
	m_currentSelection != NULL, m_currentSelection && m_currentSelection->isDir());
FileObject *target = useSelection ? m_currentSelection : m_remoteCurrentDir;
```

Then call only the existing operation:

```cpp
if (target)
	CreateDirectory(target); // IDM_POPUP_REMOTE_NEWDIR

if (target)
	CreateFile(target);      // IDM_POPUP_REMOTE_NEWFILE
```

- [ ] **Step 6: Verify and commit Task 2**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$utils = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_browser_utils.cpp /Fe:_build\tests\remote_browser_utils.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $utils
& .\_build\tests\remote_browser_utils.exe
.\build.bat -Arch x64 -Config Release
git add -- tests/remote_browser_utils.cpp src/remote_browser_utils.h src/Windows/FTPWindow.cpp
git commit -m "feat: create remote items from row menus"
```

Expected: utils test prints exit 0 and Release build succeeds.

---

### Task 3: Five-column View-only Sorting

**Files:**
- Create: `src/RemoteListSort.h`
- Create: `tests/remote_list_sort.cpp`
- Modify: `src/Windows/FTPWindow.h`
- Modify: `src/Windows/FTPWindow.cpp`

**Interfaces:**
- Consumes: 64-bit `FileObject::GetSize()`, `GetMTime()`, `GetLocalName()`, `GetPath()`, `GetMod()`, `isDir()`, `isLink()`
- Produces: `RemoteListSortItem`, `remote_list_sort_less()`, one active sort column/direction, one native header arrow

- [ ] **Step 1: Write the failing comparator test**

Create `tests/remote_list_sort.cpp`:

```cpp
#include "src/RemoteListSort.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <vector>

static RemoteListSortItem item(void *data, bool dir, LONGLONG size, DWORD modified,
	const TCHAR *name, const char *path, const char *permissions)
{
	RemoteListSortItem value{};
	value.data = data;
	value.isDirectory = dir;
	value.isLink = false;
	value.size = size;
	value.modified.dwLowDateTime = modified;
	value.name = name;
	value.path = path;
	value.permissions = permissions;
	return value;
}

int main()
{
	RemoteListSortItem dirA = item((void*)1, true, 0, 20, TEXT("目錄A"), "/目錄A", "drwxr-xr-x");
	RemoteListSortItem dirB = item((void*)2, true, 0, 10, TEXT("目錄B"), "/目錄B", "drwx------");
	RemoteListSortItem small = item((void*)3, false, 100, 30, TEXT("a.txt"), "/a.txt", "-rw-r--r--");
	RemoteListSortItem large = item((void*)4, false, 5368709120LL, 40, TEXT("b.zip"), "/b.zip", "-rw-------");

	assert(remote_list_sort_less(dirA, small, RemoteSortName, false));
	assert(!remote_list_sort_less(small, dirA, RemoteSortName, false));
	assert(remote_list_sort_less(small, large, RemoteSortSize, true));
	assert(remote_list_sort_less(large, small, RemoteSortSize, false));
	assert(remote_list_sort_less(dirB, dirA, RemoteSortModified, true));
	bool chineseForward = remote_list_sort_less(dirA, dirB, RemoteSortName, true);
	bool chineseReverse = remote_list_sort_less(dirB, dirA, RemoteSortName, true);
	assert(chineseForward != chineseReverse);
	assert(remote_list_sort_less(small, large, RemoteSortType, true));
	assert(remote_list_sort_less(large, small, RemoteSortPermissions, true) !=
		remote_list_sort_less(small, large, RemoteSortPermissions, true));

	RemoteListSortItem sameNameA = item((void*)5, false, 1, 1, TEXT("Readme"), "/A/Readme", "");
	RemoteListSortItem sameNameB = item((void*)6, false, 1, 1, TEXT("README"), "/B/README", "");
	assert(remote_list_sort_less(sameNameA, sameNameB, RemoteSortName, true));
	assert(!remote_list_sort_less(sameNameB, sameNameA, RemoteSortName, true));

	printf("remote_list_sort_exit=0\n");
	return 0;
}
```

- [ ] **Step 2: Compile and verify RED**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_list_sort.cpp /Fe:_build\tests\remote_list_sort.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $command
```

Expected: C1083 because `src/RemoteListSort.h` does not exist.

- [ ] **Step 3: Add the minimum pointer-backed comparator**

Create `src/RemoteListSort.h` with:

```cpp
#ifndef REMOTE_LIST_SORT_H
#define REMOTE_LIST_SORT_H

#include <windows.h>
#include <shlwapi.h>
#include <string.h>
#include <tchar.h>

enum RemoteListSortColumn {
	RemoteSortNone = -1,
	RemoteSortName = 0,
	RemoteSortSize,
	RemoteSortModified,
	RemoteSortType,
	RemoteSortPermissions
};

struct RemoteListSortItem {
	void *data;
	bool isDirectory;
	bool isLink;
	LONGLONG size;
	FILETIME modified;
	const TCHAR *name;
	const char *path;
	const char *permissions;
};

static inline int remote_list_compare_text(const TCHAR *left, const TCHAR *right)
{
	left = left ? left : TEXT("");
	right = right ? right : TEXT("");
	int result = CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, left, -1, right, -1);
	return result ? result - CSTR_EQUAL : lstrcmpi(left, right);
}

static inline const TCHAR *remote_list_type_value(const RemoteListSortItem &item)
{
	if (item.isDirectory)
		return TEXT("");
	const TCHAR *extension = item.name ? PathFindExtension(item.name) : NULL;
	if (extension && extension[0] == TEXT('.') && extension[1] && extension != item.name)
		return extension + 1;
	return item.isLink ? TEXT("LINK") : TEXT("");
}

static inline bool remote_list_sort_less(const RemoteListSortItem &left,
	const RemoteListSortItem &right, int column, bool ascending)
{
	if (left.isDirectory != right.isDirectory)
		return left.isDirectory;

	int result = 0;
	switch (column) {
		case RemoteSortSize:
			if (!left.isDirectory)
				result = left.size < right.size ? -1 : (left.size > right.size ? 1 : 0);
			break;
		case RemoteSortModified:
			result = CompareFileTime(&left.modified, &right.modified);
			break;
		case RemoteSortType:
			result = remote_list_compare_text(remote_list_type_value(left), remote_list_type_value(right));
			break;
		case RemoteSortPermissions:
			result = lstrcmpiA(left.permissions ? left.permissions : "", right.permissions ? right.permissions : "");
			break;
		case RemoteSortName:
		default:
			result = remote_list_compare_text(left.name, right.name);
			break;
	}

	if (result == 0)
		result = remote_list_compare_text(left.name, right.name);
	if (result == 0)
		result = strcmp(left.path ? left.path : "", right.path ? right.path : "");
	return ascending ? result < 0 : result > 0;
}

#endif //REMOTE_LIST_SORT_H
```

- [ ] **Step 4: Add sort state and build a view-only vector**

In `FTPWindow`, add fields initialized to no active sort:

```cpp
int m_remoteSortColumn;
bool m_remoteSortAscending;
```

Constructor initialization:

```cpp
m_remoteSortColumn(RemoteSortNone),
m_remoteSortAscending(true),
```

In `FillRemoteList()`, insert `..` before the sortable vector, collect only matching children, and sort keys without calling `FileObject::Sort()`:

```cpp
std::vector<RemoteListSortItem> items;
for (int i = 0; i < m_remoteCurrentDir->GetChildCount(); i++) {
	FileObject *child = m_remoteCurrentDir->GetChild(i);
	if (!remote_browser_matches_filter(child->GetLocalName(), filter))
		continue;
	RemoteListSortItem item{};
	item.data = child;
	item.isDirectory = child->isDir();
	item.isLink = child->isLink();
	item.size = child->GetSize();
	item.modified = child->GetMTime();
	item.name = child->GetLocalName();
	item.path = child->GetPath();
	item.permissions = child->GetMod();
	items.push_back(item);
}

if (m_remoteSortColumn != RemoteSortNone) {
	std::sort(items.begin(), items.end(), [this](const RemoteListSortItem &left, const RemoteListSortItem &right) {
		return remote_list_sort_less(left, right, m_remoteSortColumn, m_remoteSortAscending);
	});
}

for (size_t i = 0; i < items.size(); i++) {
	FileObject *child = (FileObject*)items[i].data;
	InsertRemoteListObject(m_remoteList, &m_treeimagelist, child, child->GetLocalName(), true);
}
```

- [ ] **Step 5: Toggle one column and preserve header formatting**

Handle `LVN_COLUMNCLICK`:

```cpp
NMLISTVIEW *column = (NMLISTVIEW*)lParam;
if (m_remoteSortColumn == column->iSubItem)
	m_remoteSortAscending = !m_remoteSortAscending;
else {
	m_remoteSortColumn = column->iSubItem;
	m_remoteSortAscending = true;
}
UpdateRemoteSortHeader();
FillRemoteList();
result = TRUE;
```

Implement `UpdateRemoteSortHeader()` using the native header and preserving all non-arrow format bits:

```cpp
HWND header = ListView_GetHeader(m_remoteList);
for (int i = REMOTE_COLUMN_NAME; i <= REMOTE_COLUMN_PERMISSIONS; i++) {
	HDITEM item{};
	item.mask = HDI_FORMAT;
	if (!Header_GetItem(header, i, &item))
		continue;
	item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
	if (i == m_remoteSortColumn)
		item.fmt |= m_remoteSortAscending ? HDF_SORTUP : HDF_SORTDOWN;
	Header_SetItem(header, i, &item);
}
return 0;
```

Declare `UpdateRemoteSortHeader()` in `FTPWindow.h`. Do not reset sort state in `SetRemoteCurrentDir()`, refresh, filter, or mutation focus restore.

- [ ] **Step 6: Run comparator and Release verification**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_list_sort.cpp /Fe:_build\tests\remote_list_sort.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $command
& .\_build\tests\remote_list_sort.exe
.\build.bat -Arch x64 -Config Release
git -c core.whitespace=cr-at-eol diff --check
```

Expected: `remote_list_sort_exit=0`; Release build succeeds; Size remains right-aligned because header updates only clear/set arrow bits.

- [ ] **Step 7: Commit Task 3**

```powershell
git add -- src/RemoteListSort.h tests/remote_list_sort.cpp src/Windows/FTPWindow.h src/Windows/FTPWindow.cpp
git commit -m "feat: sort flat remote list columns"
```

---

### Task 4: Full Regression, Manual FTP/SFTP QA, and Documentation

**Files:**
- Modify: `README.md`
- Modify: `history.md`
- Modify if applicable: `todo.md`

**Interfaces:**
- Consumes: Tasks 1-3 and the completed 64-bit plan
- Produces: verified DLL/ZIP and documentation matching shipped behavior

- [ ] **Step 1: Run every focused regression**

```powershell
& .\_build\tests\file_size_64.exe
& .\_build\tests\remote_browser_utils.exe
& .\_build\tests\remote_list_keyboard.exe
& .\_build\tests\remote_list_sort.exe
& .\_build\tests\utcp_transfer_buffer.exe
& .\_build\tests\recent_dirs.exe
```

Expected: every executable prints its existing `*_exit=0` marker.

- [ ] **Step 2: Build x64 Release**

```powershell
.\build.bat -Arch x64 -Config Release
Get-FileHash -Algorithm SHA256 -LiteralPath '_build\NppFTP-0.30.22-win64.zip'
```

Expected: `_build\Release\NppFTP.dll` and `_build\NppFTP-0.30.22-win64.zip` exist; SHA256 is printed.

- [ ] **Step 3: Deploy to the local Notepad++ test installation**

```powershell
& .\copyNppFTPdllToRealENV.bat
```

Expected: script reports a successful binary copy and `fc /B` match. If Notepad++ locks the DLL, close it and rerun elevated; do not terminate unrelated processes.

- [ ] **Step 4: Perform the manual acceptance matrix**

Verify on at least one real connection:

```text
1. Select a file, press Backspace: LIST parent once, then enter parent.
2. Select a directory, press Backspace: same result; root Backspace is a no-op.
3. Enter empty directory: succeeds with zero rows except '..'.
4. Enter denied/missing directory: current path/list/selection remain; one accurate modal appears.
5. Rapidly activate A then B: B wins; A cannot clear B's wait cursor or show a stale error.
6. Right-click directory: create directory/file inside it.
7. Right-click file: create directory/file in the current directory.
8. Right-click '..': no mutation menu.
9. Click each of five headers twice: asc then desc; one arrow; directories remain first; '..' remains row 0.
10. Sort Chinese/English names and a file above 4 GB: stable locale order and correct numeric Size order.
11. Rename/create then refresh while sorted: focus returns to the correct remote path.
```

Run both FTP/FTPS and SFTP when credentials are available; if only one protocol is available, record the other as pending real-server QA instead of claiming it passed.

- [ ] **Step 5: Update README/history and commit**

Add concise shipped-behavior bullets to `README.md`, record automated/manual results and ZIP SHA256 in `history.md`, and remove only completed matching items from `todo.md`.

```powershell
git diff --check
git add -- README.md history.md todo.md
git commit -m "docs: record remote list navigation and sorting"
```
