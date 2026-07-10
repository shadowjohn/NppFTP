# Recursive Directory Upload Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Recursively upload local directories into the chosen remote target, merge existing remote directories, prompt for known file collisions, update every file progress row, and show one failure summary.

**Architecture:** Build an owned upload plan, scan only remote directories that the plan enters on m_mainQueue, make overwrite choices on the UI thread, then enqueue ensure-directory and file-upload operations in parent-first order on m_transferQueue. A final no-op operation owns batch state until FTPWindow shows the summary.

**Tech Stack:** C++11 std::vector/std::basic_string, Win32 FindFirstFile/FindNextFile, FTPClientWrapper, QueueOperation, FTPQueue, assert-based test executables.

## Global Constraints

- Execute after docs/superpowers/plans/2026-07-10-flat-remote-file-actions.md.
- A local directory becomes a child directory named after its local basename below the selected remote target.
- Existing remote directories merge; no delete/recreate operation is allowed.
- Scan only recursive inputs; direct files retain cache-only conflict detection.
- Order is parent directory, child directory, then file uploads.
- QueueWindow displays real file uploads only; ensure/completion operations stay hidden.
- No recursive delete, sync, resume, or saved overwrite-all setting.

---

## File Map

- Create src/RemoteUploadPlan.h/.cpp and tests/remote_upload_plan.cpp for local/remote action planning.
- Modify src/QueueOperation.h/.cpp for scan, ensure-directory, and complete operations.
- Modify src/FTPSession.h/.cpp to use m_mainQueue only for scan and m_transferQueue only for ordered writes.
- Modify src/Windows/FTPWindow.h/.cpp to start scans, confirm conflicts, collect batch failures, and summarize.
- Modify src/Windows/QueueWindow.cpp:128-153, specifically QueueWindow::ProgressQueueItem, to update every upload row.
- CMakeLists.txt needs no edit because it globs src sources.

### Task 1: Upload Plan And Local Enumeration

**Files:**
- Create: src/RemoteUploadPlan.h
- Create: src/RemoteUploadPlan.cpp
- Create: tests/remote_upload_plan.cpp

**Interfaces:**
- Produces RemoteUploadItem, RemoteUploadPlan::Build, AddDirectory, AddFile, ApplyRemoteDirectoryListing, and GetItems.
- Consumed by scan and queue work in Task 2.

- [x] **Step 1: Write the failing order test**

Create tests/remote_upload_plan.cpp:

~~~
#include "src/RemoteUploadPlan.h"
#include <assert.h>
#include <stdio.h>

int main()
{
	RemoteUploadPlan plan;
	assert(plan.AddDirectory(TEXT("C:\\site"), "/var/www/site") == 0);
	assert(plan.AddDirectory(TEXT("C:\\site\\assets"), "/var/www/site/assets") == 0);
	assert(plan.AddFile(TEXT("C:\\site\\index.html"), "/var/www/site/index.html") == 0);
	assert(plan.AddFile(TEXT("C:\\site\\assets\\app.js"), "/var/www/site/assets/app.js") == 0);
	assert(plan.GetItems().size() == 4);
	assert(plan.GetItems()[0].isDirectory);
	assert(plan.GetItems()[1].isDirectory);
	assert(!plan.GetItems()[2].isDirectory);
	assert(!plan.GetItems()[3].isDirectory);
	printf("remote_upload_plan_exit=0\n");
	return 0;
}
~~~

- [x] **Step 2: Compile test and verify it fails**

Run:

~~~
cmd.exe /C "call \"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat\" -arch=x64 && cl /nologo /EHsc /DUNICODE /D_UNICODE /D_WIN32 /DWIN32 /I. /I.\src /Fo_build\tests\ /Fe_build\tests\remote_upload_plan.exe tests\remote_upload_plan.cpp src\RemoteUploadPlan.cpp shlwapi.lib && _build\tests\remote_upload_plan.exe"
~~~

Expected: compile failure because RemoteUploadPlan does not exist.

- [x] **Step 3: Implement the owned plan**

Use this public model:

~~~
struct RemoteUploadItem {
	bool isDirectory;
	std::basic_string<TCHAR> localPath;
	std::string remotePath;
	bool remoteFileExists;
	bool selected;
};

class RemoteUploadPlan {
public:
	int Build(const TCHAR *localDirectory, const char *remoteParent);
	int AddDirectory(const TCHAR *localPath, const char *remotePath);
	int AddFile(const TCHAR *localPath, const char *remotePath);
	int ApplyRemoteDirectoryListing(const char *directoryPath, const FTPFile *files, int count);
	const std::vector<RemoteUploadItem>& GetItems() const;
	std::vector<RemoteUploadItem>& GetItems();
private:
	int AddDirectoryRecursive(const TCHAR *localDirectory, const char *remoteDirectory);
	std::vector<RemoteUploadItem> m_items;
};
~~~

Build clears m_items, gets the local basename with PathFindFileName, joins it to remoteParent with one bounded local JoinRemotePath helper, and enumerates with FindFirstFile/FindNextFile. Skip . and .., recurse into FILE_ATTRIBUTE_DIRECTORY, and append each directory before its descendants and files. Reject local or remote paths that exceed MAX_PATH. Keeping the join helper local makes tests link only RemoteUploadPlan.cpp and shlwapi.lib.

ApplyRemoteDirectoryListing compares listed FTPFile names with plan file basenames below directoryPath. Mark matching file items remoteFileExists=true and never mark directories as overwrite candidates.

- [x] **Step 4: Add collision test and verify green**

Add a synthetic FTPFile for /var/www/site/index.html, apply it, and assert only that file has remoteFileExists. Run the Step 2 command.

Expected: remote_upload_plan_exit=0.

- [x] **Step 5: Commit**

~~~
git add src/RemoteUploadPlan.h src/RemoteUploadPlan.cpp tests/remote_upload_plan.cpp
git commit -m "feat: add remote directory upload plan"
~~~

### Task 2: Remote Scan And Ordered Transfer Queue

**Files:**
- Modify: src/QueueOperation.h:42-90, 191-445
- Modify: src/QueueOperation.cpp:402-445
- Modify: src/FTPSession.h:40-70
- Modify: src/FTPSession.cpp:231-544

**Interfaces:**
- Consumes RemoteUploadPlan and RemoteFailureKind.
- Produces QueueRemoteUploadScan, QueueEnsureDirectory, QueueRemoteUploadComplete, FTPSession::ScanRemoteUploadPlan, and FTPSession::QueueRemoteUploadPlan.

- [ ] **Step 1: Extend test for selected state**

Add:

~~~
plan.GetItems()[2].selected = false;
assert(!plan.GetItems()[2].selected);
assert(plan.GetItems()[3].selected);
~~~

- [ ] **Step 2: Run focused test**

Run the Task 1 Step 2 command.

Expected: remote_upload_plan_exit=0.

- [ ] **Step 3: Add scan, ensure, complete, and session APIs**

Append these QueueType values after QueueTypeNoOp:

~~~
QueueTypeRemoteUploadScan,
QueueTypeEnsureDirectory,
QueueTypeRemoteUploadComplete
~~~

QueueRemoteUploadScan owns a RemoteUploadPlan until ReleasePlan() is called by FTPWindow. Perform iterates planned directories, calls m_client->GetDir, applies successful listings, releases files, continues on RemoteFailureNotFound, and stops on another failure.

QueueEnsureDirectory uses this control flow:

~~~
m_result = m_client->MkDir(m_directoryPath);
if (m_result == 0)
	return 0;
FTPFile *files = NULL;
int listed = m_client->GetDir(m_directoryPath, &files);
if (listed >= 0) {
	m_client->ReleaseDir(files, listed);
	m_result = 0;
}
return m_result;
~~~

This preserves merge behavior if another client creates the directory after scanning and rejects a file at the same path.

QueueRemoteUploadComplete returns 0 and owns RemoteUploadBatch. The batch owns the plan, target path, completed count, and failure strings. It is added last so every prior operation may use its notify data.

Add:

~~~
int ScanRemoteUploadPlan(RemoteUploadPlan *plan);
int QueueRemoteUploadPlan(RemoteUploadPlan *plan);
~~~

ScanRemoteUploadPlan adds QueueRemoteUploadScan to m_mainQueue. QueueRemoteUploadPlan allocates RemoteUploadBatch, puts QueueEnsureDirectory for every directory and QueueUpload for every selected file on m_transferQueue, then adds QueueRemoteUploadComplete. Never put ensure-directory work on m_mainQueue because it races m_transferQueue uploads.

- [ ] **Step 4: Build and inspect ordering**

Run:

~~~
.\build.bat -Arch x64 -Config Release
~~~

Expected: build succeeds. During local validation, verify the order in Output is root directory, child directory, root file, child file, complete marker. Remove any temporary diagnostic lines before commit.

- [ ] **Step 5: Commit**

~~~
git add src/QueueOperation.h src/QueueOperation.cpp src/FTPSession.h src/FTPSession.cpp src/RemoteUploadPlan.h src/RemoteUploadPlan.cpp tests/remote_upload_plan.cpp
git commit -m "feat: queue recursive remote uploads in order"
~~~

### Task 3: UI Prompts, Batch Summary, And Progress Rows

**Files:**
- Modify: src/Windows/FTPWindow.h:82-175
- Modify: src/Windows/FTPWindow.cpp:780-1450, 1537-1645, 1838-2008
- Modify: src/Windows/QueueWindow.cpp:128-153
- Modify: tests/remote_upload_plan.cpp

**Interfaces:**
- Consumes RemoteUploadPlan, FTPSession scan/queue methods, OverwriteDialog, and RemoteUploadBatch.
- Produces FTPWindow::BeginRemoteDirectoryUpload, ConfirmRemoteUploadCollisions, RecordRemoteUploadFailure, and ShowRemoteUploadSummary.

- [ ] **Step 1: Add final parent-before-child assertions**

Add:

~~~
assert(plan.GetItems()[0].remotePath == "/var/www/site");
assert(plan.GetItems()[1].remotePath == "/var/www/site/assets");
assert(plan.GetItems()[2].remotePath == "/var/www/site/index.html");
assert(plan.GetItems()[3].remotePath == "/var/www/site/assets/app.js");
~~~

- [ ] **Step 2: Run focused test**

Run the Task 1 Step 2 command.

Expected: remote_upload_plan_exit=0.

- [ ] **Step 3: Wire directories into the flat browser**

Add:

~~~
int BeginRemoteDirectoryUpload(const TCHAR *localDirectory, FileObject *target);
int ConfirmRemoteUploadCollisions(RemoteUploadPlan *plan);
void RecordRemoteUploadFailure(RemoteUploadBatch *batch, const TCHAR *operation, QueueOperation *op);
void ShowRemoteUploadSummary(RemoteUploadBatch *batch);
~~~

BeginRemoteDirectoryUpload builds a plan from localDirectory and target->GetPath, then transfers ownership to ScanRemoteUploadPlan. Replace the deferred local-directory branch from the direct-actions plan in picker and OLE-drop paths.

On successful QueueTypeRemoteUploadScan, ReleasePlan, loop only remoteFileExists && selected items through OverwriteDialog, clear selected for Skip, delete plan on Cancel, and queue the selected plan on success. Preserve overwrite-all only after an Overwrite result.

For batch failures, append one Output line and one batch failure string; do not show direct-operation modals. On QueueTypeRemoteUploadComplete, display either Directory upload completed. or Directory upload completed with N failed item(s). See Output for details. Refresh batch target once. Do not request a refresh after every batch file success.

Delete only this QueueWindow block:

~~~
if (index != 0) {
	return -1;
}
~~~

Every visible QueueTypeUpload row will then update through existing pointer lookup.

- [ ] **Step 4: Verify build and real-server behavior**

Run the Task 1 test command, then:

~~~
.\build.bat -Arch x64 -Config Release
~~~

Expected: remote_upload_plan_exit=0 and a successful build. In Notepad++, verify picker and drag directory target rules, existing-directory merge, nested overwrite prompt, Skip, overwrite-all reset, every file progress row, one summary dialog, and denied/missing-path Output.

- [ ] **Step 5: Commit and record**

~~~
git add src/Windows/FTPWindow.h src/Windows/FTPWindow.cpp src/Windows/QueueWindow.cpp tests/remote_upload_plan.cpp todo.md history.md
git commit -m "feat: upload remote directories recursively"
~~~

Tick recursive upload and progress items in todo.md. Add test, build, and real-server QA evidence to history.md.
