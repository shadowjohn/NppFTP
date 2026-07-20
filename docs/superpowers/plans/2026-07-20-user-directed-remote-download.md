# User-Directed Remote Download Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Let users save a remote file to a chosen path or recursively download a remote directory into a chosen local parent without opening Notepad++.

**Architecture:** Build and scan an owned RemoteDownloadPlan on m_mainQueue before scheduling any files. The UI creates local directories, resolves local file collisions, then queues selected file downloads on m_transferQueue with a small ref-counted batch marker. Edit remains the existing cache-and-open path; every Download trigger calls one new UI helper.

**Tech Stack:** C++11 std::vector/std::basic_string, Win32 common dialogs and filesystem APIs, existing FTPClientWrapper, QueueOperation, and assert-based x64 test executables.

## Global Constraints

- Use the existing PU::GetSaveFilename and PU::BrowseDirectory dialogs; do not add a UI library or Explorer completion action.
- Download and Edit are distinct: only Edit uses NppFTP cache and opens a file.
- Directory downloads create <chosen parent>\<remote basename> and merge existing local directories; never delete or replace a directory.
- Keep every output path beneath that root, reject malformed/overlong names, and never recurse into a remote symlink.
- Resolve regular-file collisions with Overwrite, Skip, Cancel, Overwrite all, and Skip all for the current download batch only.
- Keep standalone download failures actionable; recursive downloads write all item failures to Output and display one completion summary.
- Do not add sync, resume, timestamps/permission preservation, or symlink traversal.

---

## File Map

- Create src/RemoteDownloadPlan.h/.cpp and tests/remote_download_plan.cpp for safe local path planning and focused checks.
- Modify src/QueueOperation.h/.cpp and src/FTPSession.h/.cpp for a main-queue scanner and transfer-queue batch ownership.
- Modify src/Windows/Commands.h, src/Windows/FTPWindow.h/.cpp, src/Windows/OverwriteDialog.h/.cpp, and src/Windows/NppFTP.rc for the Download UI and batch collision choices.
- Modify README.md, todo.md, and history.md only after the feature is built and checked. CMakeLists.txt needs no edit because source files are already globbed.

### Task 1: Safe Remote Download Plan

**Files:**
- Create: src/RemoteDownloadPlan.h
- Create: src/RemoteDownloadPlan.cpp
- Create: tests/remote_download_plan.cpp

**Interfaces:**
- Produces RemoteDownloadItem, RemoteDownloadPlan::Build, ApplyRemoteDirectoryListing, PrepareLocalDirectories, GetLocalFileCollisions, GetItems, and GetFailures.
- Consumed by QueueRemoteDownloadScan and FTPWindow in Tasks 2 and 3.

- [ ] **Step 1: Write the failing plan test**

Create tests/remote_download_plan.cpp with these initial assertions:

~~~
#include "src/RemoteDownloadPlan.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    const TCHAR * parent = TEXT("_build\\tests\\remote_download_fixture");
    RemoveDirectory(parent);
    assert(CreateDirectory(TEXT("_build\\tests"), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    assert(CreateDirectory(parent, NULL));

    RemoteDownloadPlan plan;
    assert(plan.Build(parent, "/var/www/html") == 0);
    assert(plan.GetItems().size() == 1);
    assert(plan.GetItems()[0].isDirectory);
    assert(plan.GetItems()[0].remotePath == "/var/www/html");
    assert(plan.GetItems()[0].localPath == TEXT("_build\\tests\\remote_download_fixture\\html"));

    FTPFile files[3]{};
    lstrcpynA(files[0].filePath, "/var/www/html/index.html", MAX_PATH);
    files[0].fileType = FTPTypeFile;
    lstrcpynA(files[1].filePath, "/var/www/html/assets", MAX_PATH);
    files[1].fileType = FTPTypeDir;
    lstrcpynA(files[2].filePath, "/var/www/html/current", MAX_PATH);
    files[2].fileType = FTPTypeLink;
    assert(plan.ApplyRemoteDirectoryListing("/var/www/html", files, 3) == 0);
    assert(plan.GetItems().size() == 3);
    assert(plan.GetItems()[1].localPath == TEXT("_build\\tests\\remote_download_fixture\\html\\index.html"));
    assert(plan.GetItems()[2].localPath == TEXT("_build\\tests\\remote_download_fixture\\html\\assets"));
    assert(plan.GetFailures().size() == 1);

    printf("remote_download_plan_exit=0\n");
    return 0;
}
~~~

- [ ] **Step 2: Compile the test and verify red**

Run:

~~~powershell
$dev = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
$command = '"' + $dev + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /D_WIN32 /DWIN32 /I. /I.\src tests\remote_download_plan.cpp src\RemoteDownloadPlan.cpp /Fo:_build\tests\ /Fe:_build\tests\remote_download_plan.exe shlwapi.lib shell32.lib user32.lib'
cmd.exe /C $command
~~~

Expected: compilation fails because src/RemoteDownloadPlan.h does not exist.

- [ ] **Step 3: Implement the smallest standalone plan**

Define this public model in src/RemoteDownloadPlan.h:

~~~cpp
struct RemoteDownloadItem {
    bool isDirectory;
    bool isLink;
    bool selected;
    bool scanned;
    std::basic_string<TCHAR> localPath;
    std::string remotePath;
};

class RemoteDownloadPlan {
public:
    int Build(const TCHAR * localParent, const char * remoteRoot);
    int ApplyRemoteDirectoryListing(const char * directoryPath, const FTPFile * files, int count);
    int PrepareLocalDirectories();
    void GetLocalFileCollisions(std::vector<size_t> & indices) const;
    const std::vector<RemoteDownloadItem> & GetItems() const;
    std::vector<RemoteDownloadItem> & GetItems();
    const std::vector<std::basic_string<TCHAR> > & GetFailures() const;
private:
    int AddItem(bool isDirectory, bool isLink, const char * remotePath);
    void AddFailure(const TCHAR * message);
    std::vector<RemoteDownloadItem> m_items;
    std::vector<std::basic_string<TCHAR> > m_failures;
    std::basic_string<TCHAR> m_localRoot;
    std::string m_remoteRoot;
};
~~~

Build normalizes a non-root remote directory and makes its basename a child of localParent. ApplyRemoteDirectoryListing accepts only immediate child paths of directoryPath, validates every path segment before joining with PathCombine, and records malformed, escaping, or overlong names as failures without adding an item. A link becomes an unselected failure item and is never scanned. Directory items are appended before their descendants and files. Keep the path join helper private to this file so the test links only the plan and Win32 libraries.

PrepareLocalDirectories iterates selected directories in plan order, creates missing directories with a private `SHCreateDirectoryEx` helper, and marks a directory and its descendants unselected when a regular file blocks that path. The helper records an error in the plan instead of using `OutErr`, so the focused plan test stays linkable without the full plugin. GetLocalFileCollisions returns selected non-link file indexes where GetFileAttributes reports an existing regular file. Both methods append clear local-path failures for the later batch summary.

- [ ] **Step 4: Extend the test for nesting, safety, and local collisions**

Append to tests/remote_download_plan.cpp:

~~~cpp
FTPFile nested{};
lstrcpynA(nested.filePath, "/var/www/html/assets/app.js", MAX_PATH);
nested.fileType = FTPTypeFile;
assert(plan.ApplyRemoteDirectoryListing("/var/www/html/assets", &nested, 1) == 0);
assert(plan.GetItems()[3].localPath == TEXT("_build\\tests\\remote_download_fixture\\html\\assets\\app.js"));

FTPFile escape{};
lstrcpynA(escape.filePath, "/var/www/html/../escape.txt", MAX_PATH);
escape.fileType = FTPTypeFile;
assert(plan.ApplyRemoteDirectoryListing("/var/www/html", &escape, 1) == 0);
assert(plan.GetItems().size() == 4);
assert(plan.GetFailures().size() == 2);

assert(plan.PrepareLocalDirectories() == 0);
HANDLE existing = CreateFile(plan.GetItems()[1].localPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
assert(existing != INVALID_HANDLE_VALUE);
CloseHandle(existing);
std::vector<size_t> collisions;
plan.GetLocalFileCollisions(collisions);
assert(collisions.size() == 1 && collisions[0] == 1);

assert(DeleteFile(plan.GetItems()[3].localPath.c_str()));
assert(DeleteFile(plan.GetItems()[1].localPath.c_str()));
assert(RemoveDirectory(plan.GetItems()[2].localPath.c_str()));
assert(RemoveDirectory(plan.GetItems()[0].localPath.c_str()));
assert(RemoveDirectory(parent));
~~~

- [ ] **Step 5: Compile green and commit**

Run the Step 2 command, then:

~~~powershell
& .\_build\tests\remote_download_plan.exe
~~~

Expected: remote_download_plan_exit=0.

~~~powershell
git add src/RemoteDownloadPlan.h src/RemoteDownloadPlan.cpp tests/remote_download_plan.cpp
git commit -m "feat: add remote download plan"
~~~

### Task 2: Scan And Queue A Download Batch

**Files:**
- Modify: src/QueueOperation.h
- Modify: src/QueueOperation.cpp
- Modify: src/FTPSession.h
- Modify: src/FTPSession.cpp
- Modify: src/RemoteDownloadPlan.h
- Modify: src/RemoteDownloadPlan.cpp
- Modify: tests/remote_download_plan.cpp

**Interfaces:**
- Consumes RemoteDownloadPlan from Task 1.
- Produces QueueRemoteDownloadScan, QueueRemoteDownloadFile, QueueRemoteDownloadComplete, RemoteDownloadBatch, FTPSession::ScanRemoteDownloadPlan, and FTPSession::QueueRemoteDownloadPlan.

- [ ] **Step 1: Extend the focused test for batch ownership**

Add this near the end of tests/remote_download_plan.cpp:

~~~cpp
RemoteDownloadPlan * owned = new RemoteDownloadPlan;
RemoteDownloadBatch * batch = new RemoteDownloadBatch(owned);
assert(batch->plan == owned);
assert(batch->completedCount == 0);
batch->AddRef();
batch->Release();
batch->Release();
~~~

- [ ] **Step 2: Verify the test is red**

Run the Task 1 compile command and executable.

Expected: compilation fails because RemoteDownloadBatch is undeclared.

- [ ] **Step 3: Add queue ownership and session APIs**

Append these queue types after the existing remote upload types:

~~~cpp
QueueTypeRemoteDownloadScan,
QueueTypeRemoteDownloadComplete
~~~

Add this batch model to src/RemoteDownloadPlan.h and implement its destructor
and interlocked reference methods in src/RemoteDownloadPlan.cpp:

~~~cpp
struct RemoteDownloadBatch {
    RemoteDownloadBatch(RemoteDownloadPlan * downloadPlan);
    ~RemoteDownloadBatch();
    void AddRef();
    void Release();

    RemoteDownloadPlan * plan;
    int completedCount;
    std::vector<std::basic_string<TCHAR> > failures;
private:
    volatile LONG m_references;
    RemoteDownloadBatch(const RemoteDownloadBatch &);
    RemoteDownloadBatch & operator=(const RemoteDownloadBatch &);
};
~~~

Add these classes to src/QueueOperation.h and mirror the upload batch reference-count pattern in src/QueueOperation.cpp:

~~~cpp
class QueueRemoteDownloadScan : public QueueOperation {
public:
    QueueRemoteDownloadScan(HWND hNotify, RemoteDownloadPlan * plan, int notifyCode = 0);
    ~QueueRemoteDownloadScan();
    int Perform();
    bool Equals(const QueueOperation & other);
    RemoteDownloadPlan * ReleasePlan();
private:
    RemoteDownloadPlan * m_plan;
};

class QueueRemoteDownloadFile : public QueueDownload {
public:
    QueueRemoteDownloadFile(HWND hNotify, const char * remote, const TCHAR * local,
        Transfer_Mode mode, RemoteDownloadBatch * batch, int notifyCode = 1);
    ~QueueRemoteDownloadFile();
private:
    RemoteDownloadBatch * m_batch;
};

class QueueRemoteDownloadComplete : public QueueOperation {
public:
    QueueRemoteDownloadComplete(HWND hNotify, RemoteDownloadBatch * batch, int notifyCode = 0);
    ~QueueRemoteDownloadComplete();
    int Perform();
    bool Equals(const QueueOperation & other);
    RemoteDownloadBatch * GetBatch() const;
private:
    RemoteDownloadBatch * m_batch;
};
~~~

QueueRemoteDownloadScan::Perform loops from index zero while the plan grows. For each selected, non-link, unscanned directory it calls m_client->GetDir, hands the listing to ApplyRemoteDirectoryListing, releases the listing, and marks that directory scanned. A failed listing fails the whole pre-transfer scan; no file is queued. It must call Connect only through the normal m_doConnect path, exactly like QueueRemoteUploadScan.

Add these session APIs:

~~~cpp
int ScanRemoteDownloadPlan(RemoteDownloadPlan * plan);
int QueueRemoteDownloadPlan(RemoteDownloadPlan * plan);
~~~

The scan uses m_mainQueue. The queue method allocates RemoteDownloadBatch, copies plan failures into its summary, creates one QueueRemoteDownloadFile per selected non-link file on m_transferQueue, appends one completion operation, then releases its initial batch reference. Obtain each transfer mode from PU::FindLocalFilename(item.localPath.c_str()); do not queue local directory work on m_transferQueue.

- [ ] **Step 4: Run focused test and Release build**

Run:

~~~powershell
& .\_build\tests\remote_download_plan.exe
.\build.bat -Arch x64 -Config Release
~~~

Expected: remote_download_plan_exit=0 and a successful x64 Release build.

- [ ] **Step 5: Commit**

~~~powershell
git add src/QueueOperation.h src/QueueOperation.cpp src/FTPSession.h src/FTPSession.cpp src/RemoteDownloadPlan.h src/RemoteDownloadPlan.cpp tests/remote_download_plan.cpp
git commit -m "feat: queue recursive remote downloads"
~~~

### Task 3: Route Download UI And Complete Batches

**Files:**
- Modify: src/Windows/Commands.h
- Modify: src/Windows/FTPWindow.h
- Modify: src/Windows/FTPWindow.cpp
- Modify: src/Windows/OverwriteDialog.h
- Modify: src/Windows/OverwriteDialog.cpp
- Modify: src/Windows/NppFTP.rc
- Modify: tests/remote_download_plan.cpp

**Interfaces:**
- Consumes the plan and queue/session APIs from Tasks 1 and 2.
- Produces FTPWindow::PromptRemoteDownload, BeginRemoteDirectoryDownload, ConfirmRemoteDownloadCollisions, RecordRemoteDownloadFailure, and ShowRemoteDownloadSummary.

- [ ] **Step 1: Add the batch-selection assertions**

Extend tests/remote_download_plan.cpp with the interface that UI code will use after scanning:

~~~cpp
assert(plan.GetItems()[1].selected);
plan.GetItems()[1].selected = false;
assert(!plan.GetItems()[1].selected);
plan.GetItems()[1].selected = true;
~~~

Run the focused executable before changing UI code.

Expected: it remains green; this guards the Skip path used by the dialog.

- [ ] **Step 2: Make the existing overwrite dialog apply either choice to a batch**

Keep the existing numeric resource IDs, change the checkbox copy in src/Windows/NppFTP.rc to:

~~~rc
AUTOCHECKBOX "Apply this choice to all remaining files", IDC_CHECK_OVERWRITE_ALL, 6, 33, 184, 10
~~~

In OverwriteDialog, store checkbox state for both buttons and expose:

~~~cpp
bool GetApplyToAll() const;
bool GetOverwriteAll() const; // existing upload caller remains compatible
~~~

IDC_BUTTON_OVERWRITE and IDC_BUTTON_SKIP both set m_applyToAll from the checkbox. Existing upload callers use GetOverwriteAll only after an Overwrite decision, preserving their session-wide overwrite behavior. The new download code interprets GetApplyToAll after Skip as Skip all for this one batch.

- [ ] **Step 3: Add one Download entry point and menu routing**

Add IDM_POPUP_REMOTE_DOWNLOAD to src/Windows/Commands.h. Declare:

~~~cpp
int PromptRemoteDownload(FileObject * fo);
int BeginRemoteDirectoryDownload(const TCHAR * localParent, FileObject * target);
int ConfirmRemoteDownloadCollisions(RemoteDownloadPlan * plan);
void RecordRemoteDownloadFailure(RemoteDownloadBatch * batch, QueueOperation * op);
void ShowRemoteDownloadSummary(RemoteDownloadBatch * batch);
~~~

Add Download... to both m_popupRemoteFile and m_popupRemoteDir. Remove the duplicate old-tree Save file as... menu row; route its command ID, the old Download command, the new flat command, and the toolbar button to PromptRemoteDownload(m_currentSelection). Enable the toolbar for either a selected remote file or directory.

Implement the shared helper as:

~~~cpp
int FTPWindow::PromptRemoteDownload(FileObject * fo)
{
    if (!fo || !m_ftpSession || !m_ftpSession->IsConnected())
        return -1;
    if (fo->isDir()) {
        TCHAR parent[MAX_PATH]{};
        if (PU::BrowseDirectory(parent, MAX_PATH, m_hwnd) != 0)
            return 0;
        return BeginRemoteDirectoryDownload(parent, fo);
    }
    TCHAR target[MAX_PATH]{};
    lstrcpyn(target, fo->GetLocalName(), MAX_PATH);
    if (PU::GetSaveFilename(target, MAX_PATH, m_hwnd) != 0)
        return 0;
    return m_ftpSession->DownloadFile(fo->GetPath(), target, false, 1);
}
~~~

Do not call DownloadFileCache from any Download command. Keep IDM_POPUP_REMOTE_EDIT and ActivateRemoteListSelection unchanged.

- [ ] **Step 4: Handle scan, collisions, failures, and no-open completion**

On successful QueueTypeRemoteDownloadScan, release the plan, call PrepareLocalDirectories, then walk GetLocalFileCollisions in order. Use two local booleans for overwriteAll and skipAll; do not use m_overwriteAll. On Skip mark the item unselected; on Cancel delete the plan and log cancellation. Call QueueRemoteDownloadPlan only after all choices are complete.

In the existing QueueTypeDownload completion branch, distinguish batch data from ordinary saves:

~~~cpp
RemoteDownloadBatch * batch = (RemoteDownloadBatch*)data;
if (queueResult == -1) {
    if (batch) RecordRemoteDownloadFailure(batch, queueOp);
    else ShowOperationFailure(queueOp);
} else if (batch) {
    batch->completedCount++;
} else if (code == 0) {
    SendMessage(m_hNpp, NPPM_DOOPEN, 0, (LPARAM)opdld->GetLocalPath());
} else {
    OutMsg("[FTPWindow] Download of %T succeeded.", SU::Utf8ToTChar(opdld->GetExternalPath()));
}
~~~

This removes the existing post-download open question for all user-selected saves and preserves automatic cache open only for Edit. On QueueTypeRemoteDownloadComplete, show one information or warning summary and write individual failure paths/reasons to Output. A scan failure or local-root failure shows one error before transfers start.

- [ ] **Step 5: Build, test, and commit**

Run:

~~~powershell
& .\_build\tests\remote_download_plan.exe
.\build.bat -Arch x64 -Config Release
~~~

Expected: focused test passes and the Release build succeeds.

~~~powershell
git add src/Windows/Commands.h src/Windows/FTPWindow.h src/Windows/FTPWindow.cpp src/Windows/OverwriteDialog.h src/Windows/OverwriteDialog.cpp src/Windows/NppFTP.rc tests/remote_download_plan.cpp
git commit -m "feat: save remote files and folders locally"
~~~

### Task 4: Document And Manually Verify The Workflow

**Files:**
- Modify: README.md
- Modify: todo.md
- Modify: history.md

**Interfaces:**
- Consumes the completed Download workflow from Tasks 1-3.
- Produces current user instructions, QA status, and build evidence.

- [ ] **Step 1: Add the manual QA checklist before release**

Add this unchecked item to the remote-download TODO section:

~~~markdown
- [ ] Run manual QA: file Save As cancel/success/no-open; folder picker root creation; nested files; overwrite/skip/all/cancel; local file/directory conflicts; scan failure; FTP/FTPS/SFTP queue progress and one summary.
~~~

- [ ] **Step 2: Record the actual workflow in README and history**

Document that Download... saves files through Windows Save As, recursively downloads directories below a selected parent, never opens Notepad++ after completion, and skips remote symlink traversal. Record the exact focused test output, Release command, ZIP SHA-256, and pending real-server QA in history.md.

- [ ] **Step 3: Run the final verification set**

Run:

~~~powershell
git diff --check
& .\_build\tests\remote_download_plan.exe
& .\_build\tests\remote_browser_utils.exe
& .\_build\tests\remote_list_keyboard.exe
.\build.bat -Arch x64 -Config Release
Get-FileHash .\_build\NppFTP-0.30.22-win64.zip -Algorithm SHA256
~~~

Expected: no diff errors; each focused test prints its *_exit=0 marker; the Release package is created. Do not claim FTP/FTPS/SFTP real-server QA without the user performing it.

- [ ] **Step 4: Commit**

~~~powershell
git add README.md todo.md history.md
git commit -m "docs: describe remote download workflow"
~~~
