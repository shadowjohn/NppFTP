# Concurrent Uploads And Save Priority Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users choose one to eight simultaneous uploads globally, keep automatic uploads triggered by saving a remote file ahead of waiting manual uploads, and present the Global settings dialog in Traditional Chinese.

**Architecture:** Keep the established serial `FTPQueue` instances for browsing, mutations, and downloads. Add one dedicated `ConcurrentUploadScheduler` to each `FTPSession`; it owns cloned FTP/FTPS/SFTP wrappers, one shared urgent lane, one shared normal lane, and one active operation per worker. Manual uploads enter the normal lane. Only `UploadFileCache` enters the urgent lane. Recursive uploads retain serial, parent-first remote directory preparation on the existing transfer queue, then dispatch their selected file operations to the scheduler. A batch completion notification is emitted only after every selected file has reached a terminal state.

**Tech Stack:** Existing Win32/C++11 plugin, `FTPClientWrapper` clones, TinyXML settings persistence, Notepad++ notifications, CMake/MSVC x64 build.

## Global Constraints

- The upload limit is a global setting for FTP, FTPS, and SFTP. Valid values are 1 through 8; a missing or invalid value resolves to 1.
- A changed limit applies to the next session or reconnect. Existing worker connections are not resized live.
- Only uploads use the concurrent scheduler. Download, directory listing, metadata, copy, rename, chmod, delete, and cache browsing stay on their current serial queues.
- Active transfers are never interrupted for priority, limit changes, or promotion. Priority only changes the order of work that has not started.
- `NPPN_FILESAVED -> NppFTP::OnSave -> FTPSession::UploadFileCache` is the only urgent producer. Toolbar, picker, drag-drop, and recursive upload actions remain normal.
- A save for a waiting identical upload promotes that existing operation instead of adding another. A save during an active identical upload queues one urgent follow-up upload.
- The Global settings dialog uses Traditional Chinese for its caption and all fixed labels in this dialog. Full application localization, language selection, and translations outside this dialog are explicitly out of scope.
- Preserve all existing file encodings. `src/Windows/NppFTP.rc` may be changed to UTF-8 only because this feature adds Chinese resources; set the resource code page explicitly and prove the resource compiles in the normal build.
- Add no external dependencies. Do not alter the public profile XML contract other than the one documented global settings attribute.

---

### Task 1: Add a Testable Upload Scheduling Policy

**Files:**
- Create: `src/UploadSchedulingPolicy.h`
- Create: `src/ConcurrentUploadScheduler.h`
- Create: `src/ConcurrentUploadScheduler.cpp`
- Create: `tests/concurrent_upload_scheduler.cpp`

- [ ] Write the failing policy tests before adding worker threads. The test must use lightweight fake queue operations and prove the following ordering and deduplication contract:

```cpp
// Normal work remains FIFO. Urgent work is selected before normal work.
policy.Push(normalA, UploadPriorityNormal);
assert(policy.TakeNext() == normalA);
policy.Push(normalB, UploadPriorityNormal);
policy.Push(urgentSave, UploadPriorityUrgent);
assert(policy.TakeNext() == urgentSave);
assert(policy.TakeNext() == normalB);

// A waiting matching save promotes, while an active match is not duplicated.
assert(policy.PromoteWaiting(normalB) == true);
assert(policy.ContainsWaiting(normalB) == true);
assert(policy.PromoteWaiting(activeUpload) == false);
```

- [ ] Define a header-only `UploadSchedulingPolicy` over forward-declared `QueueOperation` pointers. It owns the urgent and normal deques, exposes `Push`, `TakeNext`, `PromoteWaiting`, `ContainsWaiting`, and `Remove`, and performs no socket, thread, or UI work. The focused test uses distinct non-null pointer values only; it must not need a live FTP operation.
- [ ] Define the production-facing scheduler contract in `ConcurrentUploadScheduler.h`:

```cpp
enum UploadPriority {
    UploadPriorityNormal,
    UploadPriorityUrgent
};

class ConcurrentUploadScheduler {
public:
    ConcurrentUploadScheduler(HWND hNotify, FTPClientWrapper *prototype, int workerCount);
    ~ConcurrentUploadScheduler();

    int Initialize();
    int Deinitialize();
    int AddQueueOp(QueueOperation *op, UploadPriority priority);
    int CancelQueueOp(QueueOperation *op);
    int AbortActive();
    int GetQueueSize() const;
    int GetActiveCount() const;
};
```

- [ ] Use `UploadSchedulingPolicy` inside the scheduler behind its existing synchronization boundary. It must remove only non-running duplicates and expose one locked `TakeNext()` selection point for workers.
- [ ] Make the scheduler the owner of operation lifecycle and preserve the existing queue notification order: `Add`, `Start`, `End`, `Remove`. A notification must still wait for the UI acknowledgement before a worker proceeds.
- [ ] Give each worker exactly one `FTPClientWrapper::Clone()` created from the session prototype. Bind an operation to that worker only at dispatch time, rather than at enqueue time.
- [ ] Forward per-worker progress through a monitor that reports the worker's active operation. Do not report the same progress to two operations.
- [ ] Make initialization, shutdown, queue access, and progress access race-safe with the repository's existing Win32 synchronization style. On shutdown: stop accepting/dispatching work, abort active wrappers, wait for all worker threads, disconnect and delete every clone, then delete pending operations.
- [ ] Compile and run the focused scheduling-policy test. Record the exact command in a short comment at the top of the test if the repository has no common test runner.

Expected focused test command:

```powershell
cl /nologo /EHsc /I src tests\concurrent_upload_scheduler.cpp /Fe:_build\tests\concurrent_upload_scheduler.exe
& .\_build\tests\concurrent_upload_scheduler.exe
```

- [ ] Build the plugin after the focused test:

```powershell
.\build.bat -Arch x64 -Config Release
```

- [ ] Commit this independently testable scheduler foundation with only its source and test files staged.

### Task 2: Persist the Global Limit and Localize Global Settings

**Files:**
- Modify: `src/FTPSettings.h`
- Modify: `src/FTPSettings.cpp`
- Modify: `src/Windows/SettingsDialog.h`
- Modify: `src/Windows/SettingsDialog.cpp`
- Modify: `src/Windows/NppFTP.rc`
- Modify: `src/Windows/resource.h`
- Modify: `tests/concurrent_upload_scheduler.cpp`

- [ ] Extend `FTPSettings` with `m_maxConcurrentUploads`, initialized to `1`, plus a getter and a `NormalizeConcurrentUploads` helper that returns its input only for values in `[1, 8]` and returns `1` for every other value.
- [ ] Persist the value in the existing global settings XML element as `maxConcurrentUploads`. Missing, zero, negative, or too-large historical values must load as `1`, not crash and not silently create more than eight workers.
- [ ] Extend the focused test to assert the clamp boundary and XML round trip:

```cpp
assert(FTPSettings::NormalizeConcurrentUploads(0) == 1);
assert(FTPSettings::NormalizeConcurrentUploads(1) == 1);
assert(FTPSettings::NormalizeConcurrentUploads(8) == 8);
assert(FTPSettings::NormalizeConcurrentUploads(9) == 1);
```

- [ ] Add a numeric edit control and spin control to `IDD_DIALOG_GLOBAL`, with range `1..8`, a default of `1`, and a new resource id for each control. Load the stored value in `SettingsDialog::OnInitDialog`; save the bounded value in the existing close/save command path.
- [ ] Convert only `src/Windows/NppFTP.rc` to UTF-8 if required for Chinese literals, add the resource compiler code-page directive at the beginning of that file, and leave every other source file's encoding untouched.
- [ ] Translate the Global dialog caption and all of its fixed visible strings to Traditional Chinese. Use `同時上傳數量` for the new field. Do not translate unrelated dialogs or runtime messages in this phase.
- [ ] Verify keyboard navigation stays intact: the numeric field must have a label, accept direct typing, obey the spin range, and leave the existing close button as the dialog's primary action.
- [ ] Run the focused test and a Release build. Open the built plugin once and confirm the dialog displays readable Traditional Chinese, the stored number returns after restart, and invalid typed values become `1`.
- [ ] Commit the settings/resource change separately after both checks pass.

### Task 3: Route Normal Uploads Through the Worker Scheduler

**Files:**
- Modify: `src/FTPSession.h`
- Modify: `src/FTPSession.cpp`
- Modify: `src/QueueOperation.h`
- Modify: `src/QueueOperation.cpp`
- Modify: `src/FTPWindow.cpp`
- Modify: `tests/concurrent_upload_scheduler.cpp`

- [ ] Add `ConcurrentUploadScheduler *m_uploadScheduler` to `FTPSession`. Construct it after the main session wrapper is connected, using the globally loaded worker count and cloned wrappers; continue using `m_transferQueue` for all non-upload transfer work.
- [ ] Add an explicit optional priority argument to the existing upload entry point, defaulting to normal:

```cpp
int UploadFile(const TCHAR * sourcefile,
               const char * target,
               bool targetIsDir,
               int code,
               UploadPriority priority = UploadPriorityNormal);
```

- [ ] Route `UploadFile` to `m_uploadScheduler->AddQueueOp(...)`; retain existing source/target path construction, overwrite handling, task-window notifications, and transfer options.
- [ ] Preserve normal manual behavior by keeping the default at `UploadPriorityNormal` at every toolbar, file-picker, and drag-drop caller.
- [ ] Add a terminal hook to `QueueOperation` that defaults to no action and can safely yield a one-shot follow-up UI notification after its `QueueEventEnd` acknowledgement. Use that hook only for remote upload batch completion; do not make the scheduler inspect concrete operation types.

```cpp
virtual QueueOperation *OnQueueTerminal(); // returns NULL in QueueOperation
```

- [ ] Keep `QueueUpload::Equals` as the identity test for same local and remote files. At enqueue, normal duplicate requests retain the established de-duplication behavior. An urgent request promotes a waiting match; an active match remains active and receives exactly one urgent follow-up operation.
- [ ] Make `AbortTransfer()` abort every active upload worker as well as the existing transfer wrapper. Make task-window cancel remove only a waiting selected upload and leave unrelated upload workers alone.
- [ ] Update `Clear()` and all failure exits in `StartSession()` so the scheduler is stopped and destroyed before its session wrapper can be destroyed. The shutdown order must be covered by an explicit null-safe guard.
- [ ] Extend the focused test with a worker-limit simulation: with limit `2`, no third operation may start until a completed worker releases a slot; urgent work must be selected before queued normal work when a slot opens.
- [ ] Run the focused test and `build.bat -Arch x64 -Config Release`. Manually connect to one FTP, one FTPS, and one SFTP profile where available; with the setting at `1`, verify legacy serial behavior is unchanged.
- [ ] Commit the scheduler/session routing only after the focused test and build pass.

### Task 4: Preserve Recursive Upload Ordering and Batch Completion

**Files:**
- Modify: `src/RemoteUploadPlan.h`
- Modify: `src/RemoteUploadPlan.cpp`
- Modify: `src/QueueOperation.h`
- Modify: `src/QueueOperation.cpp`
- Modify: `src/FTPSession.cpp`
- Modify: `src/FTPWindow.cpp`
- Modify: `tests/remote_upload_plan.cpp`
- Modify: `tests/concurrent_upload_scheduler.cpp`

- [ ] Replace the current queue-wide recursive upload layout with a serial prepare operation on `m_transferQueue`. It must create all remote directories parent-first, record directory failures in `RemoteUploadBatch`, and only dispatch file operations after preparation finishes.
- [ ] Add explicit batch terminal counters to `RemoteUploadBatch`: initialize the selected-file count before dispatch, decrement once after each selected file reaches its terminal notification state, and create the existing `QueueRemoteUploadComplete` notification only when that count reaches zero.
- [ ] Do not allow directory creation, a file upload, or a later unrelated queued upload to create a premature or duplicate recursive summary.
- [ ] Preserve the current summary contract: report selected files, successes, failures, skipped items, and cancellation truthfully. A failed worker connection counts only its affected operation as failed; other workers and later queued files continue.
- [ ] Cover the plan test with nested directories and selected-only input. Add assertions that remote directory preparation remains parent-first and that exactly one completion marker is requested after all selected files are terminal.
- [ ] Cover the scheduler test with two selected files finishing in opposite worker order and assert the completion marker appears once, after both have produced terminal events.
- [ ] Run both focused tests, then build Release. Manually drag a directory tree containing at least two files with limit `2`; confirm directories are created first, files can overlap, progress rows remain distinct, and one final summary appears.
- [ ] Commit the recursive batch integration separately after the test/build/manual check.

### Task 5: Make Remote-File Saves Urgent and Document the Behavior

**Files:**
- Modify: `src/FTPSession.cpp`
- Modify: `src/PluginInterface.cpp` only if the existing save notification path needs an explicit priority parameter
- Modify: `README.md`
- Modify: `todo.md`
- Modify: `history.md`
- Modify: `tests/concurrent_upload_scheduler.cpp`

- [ ] Change only `FTPSession::UploadFileCache` to call `UploadFile(..., UploadPriorityUrgent)`. Keep `NppFTP::OnSave` and `PluginInterface` behavior otherwise unchanged.
- [ ] Test these cases with the focused scheduler test:

```cpp
// Waiting manual upload is promoted instead of duplicated.
assert(enqueueSaveFor(manualWaiting) == UploadPromotion);
assert(policy.CountMatching(manualWaiting) == 1);

// Active manual upload continues; a later save receives one urgent follow-up.
assert(enqueueSaveFor(manualActive) == UploadUrgentFollowUp);
assert(policy.CountMatching(manualActive) == 2);
```

- [ ] Run an end-to-end manual regression: queue several large normal uploads at limit `1`, edit and save a cached remote file, and confirm the current upload completes, the saved file uploads next, and the remaining normal queue follows. Repeat at limit `2` to prove active workers are not interrupted and the urgent item takes the next free worker.
- [ ] Update `README.md` in Traditional Chinese with the global `1..8` setting, the next-session/reconnect rule, upload-only scope, and automatic-save priority behavior. State that language selection remains planned rather than implying full localization exists.
- [ ] Update `todo.md` to mark the completed two phases and retain the future full multi-language system as a separate pending item. Update `history.md` with the exact behavior, validation commands, and manual QA boundary.
- [ ] Run all focused tests, run `build.bat -Arch x64 -Config Release`, and inspect the final diff with:

```powershell
git -c core.whitespace=cr-at-eol diff --check
git status --short
```

- [ ] Perform a final Notepad++ QA pass: setting persistence and Chinese dialog, FTP/FTPS/SFTP manual upload, recursive upload, Abort all active uploads, cancel one waiting upload, waiting-save promotion, active-save follow-up, and clean disconnect while transfers are active.
- [ ] Commit the priority/docs/history work with only intended files staged. Push only after the user explicitly asks for it.

## Acceptance Checklist

- [ ] The Global settings dialog is readable in Traditional Chinese and persists `同時上傳數量` between plugin launches.
- [ ] Values below 1, above 8, or missing from old settings resolve to one worker.
- [ ] With limit 1, all upload behavior remains serial and compatible with existing FTP, FTPS, and SFTP sessions.
- [ ] With limits 2 through 8, only upload work is concurrent and no worker shares a connected wrapper with another worker.
- [ ] An auto-save upload never interrupts active work, jumps ahead of waiting normal uploads, promotes a waiting identical upload, and queues one final urgent follow-up when the identical upload is already active.
- [ ] Recursive upload preparation remains parent-first and produces exactly one accurate completion summary.
- [ ] Download, browsing, mutations, and existing profile behavior remain serial and unchanged.
- [ ] Focused scheduler and remote-upload-plan checks plus the Release build pass; real-server protocol and Notepad++ UI validation are recorded as manual proof.
