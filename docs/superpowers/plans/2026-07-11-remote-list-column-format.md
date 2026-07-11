# Remote List Column Format Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 將遠端清單 Size 欄位置右，並將 Modified 固定顯示為 `yyyy-MM-dd HH:mm:ss`。

**Architecture:** 保留既有 FILETIME 轉本地 SYSTEMTIME 流程，只把固定文字輸出放進既有 `remote_browser_utils.h` 以便直接測試。欄位對齊沿用現有 `InsertRemoteListColumn`，只增加一個 format 參數。

**Tech Stack:** C++17、Win32 ListView、assert-based tests、MSVC、CMake

---

### Task 1: 固定 Modified 格式

**Files:**
- Modify: `tests/remote_browser_utils.cpp`
- Modify: `src/remote_browser_utils.h`
- Modify: `src/Windows/FTPWindow.cpp`

- [x] **Step 1: Write the failing test**

在 `tests/remote_browser_utils.cpp` 加入：

```cpp
SYSTEMTIME modified{};
modified.wYear = 2026;
modified.wMonth = 7;
modified.wDay = 11;
modified.wHour = 21;
modified.wMinute = 1;
modified.wSecond = 0;
TCHAR modifiedText[32]{};
remote_browser_format_system_time(modified, modifiedText, 32);
assert(lstrcmp(modifiedText, TEXT("2026-07-11 21:01:00")) == 0);
```

- [x] **Step 2: Run test to verify it fails**

Run：

```powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /I. tests\remote_browser_utils.cpp /Fe:_build\tests\remote_browser_utils.exe shlwapi.lib user32.lib'
& cmd.exe /d /s /c $command
```

Expected: FAIL，`remote_browser_format_system_time` 尚未定義。

- [x] **Step 3: Write minimal implementation**

在 `src/remote_browser_utils.h` 的 `remote_browser_clear_text()` 後加入：

```cpp
static inline void remote_browser_format_system_time(const SYSTEMTIME & value, TCHAR * buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0)
		return;
#ifdef _MSC_VER
	_sntprintf_s(buffer, bufferCount, _TRUNCATE, TEXT("%04u-%02u-%02u %02u:%02u:%02u"),
		value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond);
#else
	_sntprintf(buffer, bufferCount, TEXT("%04u-%02u-%02u %02u:%02u:%02u"),
		value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond);
	buffer[bufferCount - 1] = 0;
#endif
}
```

在 `FormatRemoteModifiedTime()` 完成本地時間轉換後，以一行取代 locale date/time 組合：

```cpp
remote_browser_format_system_time(local, buffer, bufferCount);
```

- [x] **Step 4: Run test to verify it passes**

Run：重新執行上述編譯命令，再執行：

```powershell
_build\tests\remote_browser_utils.exe
```

Expected: `remote_browser_utils_exit=0`。

### Task 2: Size 欄位置右並驗證完整產物

**Files:**
- Modify: `src/Windows/FTPWindow.cpp`

- [x] **Step 1: Add the native column format parameter**

將既有 helper 改為：

```cpp
static void InsertRemoteListColumn(HWND list, int index, const TCHAR * text, int width, int format = LVCFMT_LEFT)
{
	LVCOLUMN lvc{};
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
	lvc.fmt = format;
	lvc.cx = width;
	lvc.pszText = (TCHAR*)text;
	ListView_InsertColumn(list, index, &lvc);
}
```

Size 欄位呼叫改為：

```cpp
InsertRemoteListColumn(m_remoteList, REMOTE_COLUMN_SIZE, TEXT("Size"), 70, LVCFMT_RIGHT);
```

- [x] **Step 2: Run focused checks**

Run：

```powershell
_build\tests\remote_browser_utils.exe
_build\tests\remote_list_keyboard.exe
_build\tests\utcp_transfer_buffer.exe
git -c core.whitespace=cr-at-eol diff --check
```

Expected: 三個測試 exit 0，diff check 無錯誤。

- [x] **Step 3: Build x64 Release**

Run：

```powershell
.\build.bat -Arch x64 -Config Release
```

Expected: `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip` 產生成功。
