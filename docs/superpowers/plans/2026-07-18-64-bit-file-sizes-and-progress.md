# 64-bit File Sizes and Progress Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make FTP/SFTP metadata, display, sorting input, transfer progress, and drag-drop preserve file sizes above 2 GB.

**Architecture:** Widen the existing bundled UTCP and NppFTP size pipeline in place from Windows `long` to signed `LONGLONG`, retaining `-1` as the unknown sentinel. Keep formatting in `remote_browser_utils.h`, add only two pure shared helpers for percentage calculation and Windows high/low size splitting, and leave the existing queue percentage/UI contract unchanged.

**Tech Stack:** C++17, Win32 `LONGLONG`/`LARGE_INTEGER`/`ULARGE_INTEGER`, bundled UTCP, libssh SFTP, assert-based MSVC tests, CMake x64 Release build

## Global Constraints

- Use signed 64-bit values end-to-end and preserve `-1` as unknown.
- Display non-directory sizes as `B / KB / MB / GB / TB`, base 1024, fixed two decimal places, right aligned.
- Use raw 64-bit bytes for sorting; never sort formatted text.
- Do not change the 64 KB transfer buffer, FTP/SFTP protocol behavior, or queue percentage UI.
- Do not add third-party dependencies.
- Preserve existing file encodings and line endings with patch-sized edits.

## File Map

- `src/file_size_utils.h`: pure 64-bit percentage and Win32 high/low split helpers shared by queue and drag-drop.
- `src/remote_browser_utils.h`: human-readable Size text only.
- `UTCP/include/utstrlst.h`, `UTCP/src/utstrlst.cpp`: 64-bit token parsing.
- `UTCP/include/ftp_c.h`, `UTCP/src/ftp_c.cpp`: 64-bit LIST metadata, `SIZE`, and FTP progress callbacks.
- `UTCP/include/ut_clnt.h`, `UTCP/src/ut_clnt.cpp`: 64-bit cumulative send/receive counters.
- `src/FTPFile.h`, `src/FileObject.*`, `src/Windows/ProfileObject.*`: application metadata storage.
- `src/ProgressMonitor.h`, `src/FTPQueue.*`, `src/FTPClientWrapper.h`, `src/FTPClientWrapperSSL.cpp`, `src/FTPClientWrapperSSH.cpp`: 64-bit progress propagation.
- `src/Windows/FTPWindow.cpp`, `src/Windows/Treeview.cpp`: drag-drop and legacy tooltip consumers.
- `tests/remote_browser_utils.cpp`, `tests/file_size_64.cpp`: focused format, parser, percentage, and high/low checks.

---

### Task 1: Format 64-bit Human-readable Sizes

**Files:**
- Modify: `tests/remote_browser_utils.cpp`
- Modify: `src/remote_browser_utils.h`

**Interfaces:**
- Consumes: Windows `LONGLONG`, existing `remote_browser_clear_text()`
- Produces: `remote_browser_format_size(bool isDir, LONGLONG size, TCHAR *buffer, size_t bufferCount)`

- [ ] **Step 1: Write the failing format assertions**

Replace the current Size assertions in `tests/remote_browser_utils.cpp` with:

```cpp
TCHAR text[64]{};
remote_browser_format_size(false, 512LL, text, 64);
assert(_tcscmp(text, TEXT("512.00 B")) == 0);
remote_browser_format_size(false, 1536LL, text, 64);
assert(_tcscmp(text, TEXT("1.50 KB")) == 0);
remote_browser_format_size(false, 18864384LL, text, 64);
assert(_tcscmp(text, TEXT("17.99 MB")) == 0);
remote_browser_format_size(false, 5368709120LL, text, 64);
assert(_tcscmp(text, TEXT("5.00 GB")) == 0);
remote_browser_format_size(false, 3298534883328LL, text, 64);
assert(_tcscmp(text, TEXT("3.00 TB")) == 0);
remote_browser_format_size(true, 5368709120LL, text, 64);
assert(_tcscmp(text, TEXT("")) == 0);
remote_browser_format_size(false, -1LL, text, 64);
assert(_tcscmp(text, TEXT("")) == 0);
```

- [ ] **Step 2: Run the focused test and verify RED**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I. tests\remote_browser_utils.cpp /Fe:_build\tests\remote_browser_utils.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $command
if ($LASTEXITCODE -eq 0) { & .\_build\tests\remote_browser_utils.exe }
```

Expected: assertion failure for `512.00 B` and/or a large value because the existing helper accepts 32-bit `long` and stops at MB.

- [ ] **Step 3: Implement the minimum formatter**

Replace `remote_browser_format_size()` with:

```cpp
static inline void remote_browser_format_size(bool isDir, LONGLONG size, TCHAR *buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0 || isDir || size < 0)
		return;

	static const TCHAR *units[] = { TEXT("B"), TEXT("KB"), TEXT("MB"), TEXT("GB"), TEXT("TB") };
	double value = (double)size;
	int unit = 0;
	while (value >= 1024.0 && unit < 4) {
		value /= 1024.0;
		unit++;
	}

#ifdef _MSC_VER
	_sntprintf_s(buffer, bufferCount, _TRUNCATE, TEXT("%.2f %s"), value, units[unit]);
#else
	_sntprintf(buffer, bufferCount, TEXT("%.2f %s"), value, units[unit]);
	buffer[bufferCount - 1] = 0;
#endif
}
```

- [ ] **Step 4: Re-run and verify GREEN**

Run the Step 2 command again.

Expected: `remote_browser_utils_exit=0`.

- [ ] **Step 5: Commit Task 1**

```powershell
git add -- src/remote_browser_utils.h tests/remote_browser_utils.cpp
git commit -m "feat: format 64-bit remote file sizes"
```

---

### Task 2: Widen FTP/SFTP Metadata and Parsers

**Files:**
- Create: `tests/file_size_64.cpp`
- Modify: `UTCP/include/utstrlst.h`
- Modify: `UTCP/src/utstrlst.cpp`
- Modify: `UTCP/include/ftp_c.h`
- Modify: `UTCP/src/ftp_c.cpp`
- Modify: `src/FTPFile.h`
- Modify: `src/FileObject.h`
- Modify: `src/FileObject.cpp`
- Modify: `src/Windows/ProfileObject.h`
- Modify: `src/Windows/ProfileObject.cpp`
- Modify: `src/FTPClientWrapperSSL.cpp`
- Modify: `src/FTPClientWrapperSSH.cpp`

**Interfaces:**
- Consumes: Task 1 formatter accepting `LONGLONG`
- Produces: `CUT_StrMethods::ParseString(..., LONGLONG*)`, `CUT_FTPClient::GetSize(..., LONGLONG*)`, `FTPFile::fileSize`, and `FileObject::GetSize()` as signed 64-bit

- [ ] **Step 1: Add the failing parser/type test**

Create `tests/file_size_64.cpp`:

```cpp
#include <windows.h>
#include <vector>
#include "UTCP/include/ftp_c.h"
#include "UTCP/include/utstrlst.h"
#include "src/FTPFile.h"
#include "src/FileObject.h"

#include <assert.h>
#include <stdio.h>
#include <type_traits>

int main()
{
	static_assert(sizeof(((CUT_DIRINFOA*)0)->fileSize) == sizeof(LONGLONG), "CUT_DIRINFOA size must be 64-bit");
	static_assert(sizeof(((FTPFile*)0)->fileSize) == sizeof(LONGLONG), "FTPFile size must be 64-bit");
	static_assert(std::is_same<decltype(((FileObject*)0)->GetSize()), LONGLONG>::value,
		"FileObject::GetSize must return 64-bit");

	LONGLONG value = 0;
	assert(CUT_StrMethods::ParseString("file 5368709120", " ", 1, &value) == UTE_SUCCESS);
	assert(value == 5368709120LL);
	assert(CUT_StrMethods::ParseString(L"file 1099511627776", L" ", 1, &value) == UTE_SUCCESS);
	assert(value == 1099511627776LL);

	FTPFile file{};
	file.fileSize = 5368709120LL;
	assert(file.fileSize == 5368709120LL);

	printf("file_size_64_exit=0\n");
	return 0;
}
```

- [ ] **Step 2: Compile and verify RED**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /D_WIN32 /DWIN32 /I. /I.\UTCP\include /I.\UTCP\src tests\file_size_64.cpp UTCP\src\utstrlst.cpp UTCP\src\ut_strop.cpp /Fe:_build\tests\file_size_64.exe user32.lib'
& cmd.exe /d /s /c $command
```

Expected: compile failure because no `LONGLONG*` overload exists and `FTPFile::fileSize` is still 32-bit.

- [ ] **Step 3: Add the 64-bit parser overloads**

Add declarations beside the existing `long*` overloads in `UTCP/include/utstrlst.h`:

```cpp
static int ParseString(LPCSTR string, LPCSTR sepChars, int index, LONGLONG *value);
static int ParseString(LPCWSTR string, LPCWSTR sepChars, int index, LONGLONG *value);
```

Add implementations beside the existing numeric overloads in `UTCP/src/utstrlst.cpp`:

```cpp
int CUT_StrMethods::ParseString(LPCSTR string, LPCSTR sepChars, int index, LONGLONG *value)
{
	char buf[60];
	if (ParseString(string, sepChars, index, buf, 60) == UTE_SUCCESS) {
		*value = _strtoi64(buf, NULL, 10);
		return UTE_SUCCESS;
	}
	return UTE_ERROR;
}

int CUT_StrMethods::ParseString(LPCWSTR string, LPCWSTR sepChars, int index, LONGLONG *value)
{
	wchar_t buf[60];
	if (ParseString(string, sepChars, index, buf, 60) == UTE_SUCCESS) {
		*value = _wcstoi64(buf, NULL, 10);
		return UTE_SUCCESS;
	}
	return UTE_ERROR;
}
```

- [ ] **Step 4: Widen the metadata declarations and accessors**

Apply these signature/type changes without changing field names:

```cpp
// UTCP/include/ftp_c.h, both CUT_DIRINFO structs
LONGLONG fileSize = 0;

// UTCP/include/ftp_c.h and UTCP/src/ftp_c.cpp
virtual int GetSize(LPCSTR path, LONGLONG *size);
virtual int GetSize(LPCWSTR path, LONGLONG *size);

// src/FTPFile.h
LONGLONG fileSize;

// src/FileObject.h and src/FileObject.cpp
virtual LONGLONG GetSize() const;
LONGLONG m_size;

// src/Windows/ProfileObject.h and src/Windows/ProfileObject.cpp
virtual LONGLONG GetSize() const;
LONGLONG m_size;
```

Parse FTP `SIZE` without truncation in `UTCP/src/ftp_c.cpp`:

```cpp
*size = _strtoi64(response, NULL, 10);
```

Remove the narrowing casts in both wrappers:

```cpp
// FTPClientWrapperSSL.cpp
ftpfile.fileSize = di.fileSize;

// FTPClientWrapperSSH.cpp
file.fileSize = sfile->size <= LLONG_MAX ? (LONGLONG)sfile->size : -1;
```

Include `<limits.h>` for `LLONG_MAX`, and change both FTP receive-side `SIZE` locals so the new pointer signature compiles:

```cpp
LONGLONG size = 0;
```

- [ ] **Step 5: Run focused test and x64 compile**

Run the Step 2 command, then:

```powershell
& .\_build\tests\file_size_64.exe
.\build.bat -Arch x64 -Config Release
```

Expected: `file_size_64_exit=0`; Release build succeeds. Task 3 removes the remaining progress/drag-drop narrowing sites.

- [ ] **Step 6: Commit Task 2**

```powershell
git add -- tests/file_size_64.cpp UTCP/include/utstrlst.h UTCP/src/utstrlst.cpp UTCP/include/ftp_c.h UTCP/src/ftp_c.cpp src/FTPFile.h src/FileObject.h src/FileObject.cpp src/Windows/ProfileObject.h src/Windows/ProfileObject.cpp src/FTPClientWrapperSSL.cpp src/FTPClientWrapperSSH.cpp
git commit -m "feat: preserve 64-bit remote file metadata"
```

---

### Task 3: Widen Transfer Progress and Drag-drop Sizes

**Files:**
- Create: `src/file_size_utils.h`
- Modify: `tests/file_size_64.cpp`
- Modify: `UTCP/include/ut_clnt.h`
- Modify: `UTCP/src/ut_clnt.cpp`
- Modify: `UTCP/include/ftp_c.h`
- Modify: `UTCP/src/ftp_c.cpp`
- Modify: `src/ProgressMonitor.h`
- Modify: `src/FTPQueue.h`
- Modify: `src/FTPQueue.cpp`
- Modify: `src/FTPClientWrapper.h`
- Modify: `src/FTPClientWrapperSSL.cpp`
- Modify: `src/FTPClientWrapperSSH.cpp`
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `src/Windows/Treeview.cpp`

**Interfaces:**
- Consumes: Task 2 `LONGLONG` metadata and `CUT_FTPClient::GetSize(..., LONGLONG*)`
- Produces: `file_size_progress_percent(LONGLONG, LONGLONG)` and `file_size_parts(LONGLONG)`; all progress callbacks accept `LONGLONG`

- [ ] **Step 1: Extend the focused test with failing progress/parts checks**

Add to `tests/file_size_64.cpp`:

```cpp
#include "src/file_size_utils.h"

assert(file_size_progress_percent(2684354560LL, 5368709120LL) == 50.0f);
assert(file_size_progress_percent(1LL, -1LL) == -1.0f);

ULARGE_INTEGER parts = file_size_parts(5368709120LL);
assert(parts.HighPart == 1);
assert(parts.LowPart == 1073741824);
parts = file_size_parts(-1LL);
assert(parts.QuadPart == 0);
```

- [ ] **Step 2: Compile and verify RED**

Run Task 2 Step 2.

Expected: C1083 because `src/file_size_utils.h` does not exist.

- [ ] **Step 3: Add the pure shared helpers**

Create `src/file_size_utils.h`:

```cpp
#ifndef FILE_SIZE_UTILS_H
#define FILE_SIZE_UTILS_H

#include <windows.h>

static inline float file_size_progress_percent(LONGLONG current, LONGLONG total)
{
	if (total <= 0)
		return -1.0f;
	return (float)((double)current / (double)total * 100.0);
}

static inline ULARGE_INTEGER file_size_parts(LONGLONG size)
{
	ULARGE_INTEGER value{};
	value.QuadPart = size < 0 ? 0 : (ULONGLONG)size;
	return value;
}

#endif //FILE_SIZE_UTILS_H
```

- [ ] **Step 4: Widen every progress callback and cumulative counter**

Change the following signatures from `long` to `LONGLONG` in declarations, overrides, and definitions:

```cpp
BOOL ReceiveFileStatus(LONGLONG bytesReceived);
BOOL SendFileStatus(LONGLONG bytesSent);
int OnDataReceived(LONGLONG received, LONGLONG total);
int OnDataSent(LONGLONG sent, LONGLONG total);
int SetCurrentTotal(LONGLONG total);
```

Apply them in `UTCP/include/ut_clnt.h`, `UTCP/src/ut_clnt.cpp`, `UTCP/include/ftp_c.h`, `UTCP/src/ftp_c.cpp`, `src/ProgressMonitor.h`, `src/FTPQueue.h`, `src/FTPQueue.cpp`, `src/FTPClientWrapper.h`, and `src/FTPClientWrapperSSL.cpp`. Change cumulative `bytesSent`, `bytesReceived`, `totalSent`, `totalReceived`, `totalSize`, and `m_currentTotal` fields/locals to `LONGLONG`.

Use the shared percentage helper in both `FTPQueue` callbacks:

```cpp
if (total <= 0)
	m_activeOp->SetProgress(-1.0f);
else
	m_activeOp->SetProgress(file_size_progress_percent(received, total));
```

For FTP local handle sizes, replace low-DWORD reads with:

```cpp
LARGE_INTEGER size{};
if (::GetFileSizeEx(hFile, &size))
	m_client.SetCurrentTotal(size.QuadPart);
```

For the SFTP upload local, use:

```cpp
LARGE_INTEGER size{};
if (::GetFileSizeEx(hFile, &size))
	totalSize = size.QuadPart;
```

For SFTP stat totals, retain only representable signed values:

```cpp
totalSize = fattr->size <= LLONG_MAX ? (LONGLONG)fattr->size : -1;
```

- [ ] **Step 5: Write both drag-drop DWORDs and widen the tree tooltip local**

In `FTPWindow::GetFileDescriptor()`:

```cpp
ULARGE_INTEGER size = file_size_parts(m_currentDropObject->GetSize());
fd->nFileSizeLow = size.LowPart;
fd->nFileSizeHigh = size.HighPart;
```

In `Treeview::OnToolTip()`, change only the local size type:

```cpp
LONGLONG fsize = fo->GetSize();
```

- [ ] **Step 6: Re-run focused and full verification**

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /D_WIN32 /DWIN32 /I. /I.\UTCP\include /I.\UTCP\src tests\file_size_64.cpp UTCP\src\utstrlst.cpp UTCP\src\ut_strop.cpp /Fe:_build\tests\file_size_64.exe user32.lib'
& cmd.exe /d /s /c $command
& .\_build\tests\file_size_64.exe
& .\_build\tests\remote_browser_utils.exe
.\build.bat -Arch x64 -Config Release
git -c core.whitespace=cr-at-eol diff --check
```

Expected: both focused tests print exit 0; `_build\Release\NppFTP.dll` and `_build\NppFTP-0.30.22-win64.zip` exist.

- [ ] **Step 7: Commit Task 3**

```powershell
git add -- src/file_size_utils.h tests/file_size_64.cpp UTCP/include/ut_clnt.h UTCP/src/ut_clnt.cpp UTCP/include/ftp_c.h UTCP/src/ftp_c.cpp src/ProgressMonitor.h src/FTPQueue.h src/FTPQueue.cpp src/FTPClientWrapper.h src/FTPClientWrapperSSL.cpp src/FTPClientWrapperSSH.cpp src/Windows/FTPWindow.cpp src/Windows/Treeview.cpp
git commit -m "feat: support 64-bit transfer progress"
```

---

### Task 4: Regression, Documentation, and Artifact Check

**Files:**
- Modify: `README.md`
- Modify: `history.md`

**Interfaces:**
- Consumes: Tasks 1-3
- Produces: documented 64-bit behavior and verified Release artifacts for the flat-list plan

- [ ] **Step 1: Run all focused regressions**

```powershell
& .\_build\tests\file_size_64.exe
& .\_build\tests\remote_browser_utils.exe
& .\_build\tests\remote_list_keyboard.exe
& .\_build\tests\utcp_transfer_buffer.exe
& .\_build\tests\recent_dirs.exe
```

Expected: every executable prints its existing `*_exit=0` marker.

- [ ] **Step 2: Build and inspect artifacts**

```powershell
.\build.bat -Arch x64 -Config Release
Get-Item -LiteralPath '_build\Release\NppFTP.dll', '_build\NppFTP-0.30.22-win64.zip' | Select-Object FullName, Length
Get-FileHash -Algorithm SHA256 -LiteralPath '_build\NppFTP-0.30.22-win64.zip'
```

Expected: build exits 0, both artifacts exist, and SHA256 is printed for `history.md`.

- [ ] **Step 3: Document shipped behavior**

Add concise bullets to `README.md` and `history.md` stating:

```markdown
- FTP/SFTP file sizes and transfer progress now preserve 64-bit values above 2 GB.
- Size uses B/KB/MB/GB/TB with two decimal places and remains right-aligned.
```

Record focused test results, Release build result, and ZIP SHA256 in `history.md`.

- [ ] **Step 4: Commit Task 4**

```powershell
git add -- README.md history.md
git commit -m "docs: record 64-bit file size support"
```
