# Bulk Upload Queue Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add FileZilla-like controls for pausing, resuming, and clearing bulk upload tasks without affecting downloads or the main FTP operation queues.

**Architecture:** `ConcurrentUploadScheduler` gains a session-scoped dispatch gate. Pausing prevents only waiting uploads from being assigned to an idle worker; an upload already running in a worker remains untouched. `FTPSession` exposes upload-only control methods, and `FTPWindow` selects a dedicated upload popup for upload rows or empty transfer-list space while preserving the old queue controls for downloads.

**Tech Stack:** C++17/Win32, existing `Monitor` synchronization, Win32 popup menus, existing `FTPQueue` lifecycle and focused CMake test executables.

**Spec:** `todo.md` section `5.2 大量上傳佇列控制（下一刀）`; `history.md` section `2026-08-19 Planned bulk upload queue controls`.

## Global Constraints

- Target is Windows Notepad++ NppFTP; retain the existing file encodings and line endings.
- The pause state is session-only and resets during disconnect/reconnect.
- Pause means "do not dispatch new waiting uploads". It must not suspend FTP, FTPS, or SFTP data streams already transferring.
- Pause, resume, and clear-all affect `ConcurrentUploadScheduler` only; downloads, directory browsing, and mutation queues remain unchanged.
- Normal/urgent priority and same-file conflict rules must remain intact when a paused scheduler resumes.
- A selected active upload is aborted through only the worker that owns it; it must not invoke the toolbar's all-transfer abort behavior.
- Recursive-upload batch terminal accounting must remain exactly once for selected, successful, failed, and canceled files.
- Add focused automated evidence before manual FTP/FTPS/SFTP QA. Do not add dependencies.

---

## File Structure

- `src/ConcurrentUploadScheduler.h`: upload scheduler pause state and upload-only cancellation interfaces.
- `src/ConcurrentUploadScheduler.cpp`: monitor-protected dispatch gate and targeted/all-upload cancellation implementation.
- `src/FTPSession.h`: public session façade for the upload scheduler controls.
- `src/FTPSession.cpp`: delegate upload-only controls to the scheduler while preserving existing generic queue behavior.
- `src/Windows/Commands.h`: command identifiers for the three upload queue menu actions.
- `src/Windows/FTPWindow.h`: popup handles and selected-operation state used by the upload context menu.
- `src/Windows/FTPWindow.cpp`: create, select, and execute upload-only context-menu commands; write compact status lines to Output.
- `tests/ftp_queue_terminal_lifecycle.cpp`: deterministic production-scheduler lifecycle tests with the existing `FakeWrapper` barriers.
- `tests/ftp_session_upload_routing.ps1`: static contract checks for session routing and queue-menu wiring.
- `todo.md`, `README.md`, `history.md`: finished scope, manual QA boundary, and verification evidence.

## Task 1: Scheduler Dispatch Gate

**Files:**
- Modify: `src/ConcurrentUploadScheduler.h`
- Modify: `src/ConcurrentUploadScheduler.cpp`
- Test: `tests/ftp_queue_terminal_lifecycle.cpp`

**Interfaces:**
- Produces `int ConcurrentUploadScheduler::SetDispatchPaused(bool paused)`.
- Produces `bool ConcurrentUploadScheduler::IsDispatchPaused() const`.
- Adds `bool m_dispatchPaused`, protected by `m_monitor`.
- `SetDispatchPaused` returns `0` when it changes/retains state on a live scheduler and `-1` after shutdown has started or before initialization completes.

- [x] **Step 1: Write the failing scheduler lifecycle test for pause, active completion, resume order, and reset**

  In `tests/ftp_queue_terminal_lifecycle.cpp`, add `TestUploadSchedulerDispatchPause()` using the existing `FakeWrapper` send barriers and `QueueUpload` operations. Start one normal upload and wait until its `SendFile` barrier is entered; enqueue a normal tail and an urgent tail; call `SetDispatchPaused(true)`; release the running operation; then assert that no second `SendFile` starts while the scheduler remains paused. Call `SetDispatchPaused(false)` and assert that the urgent tail enters `SendFile` before the normal tail. Finish both operations and verify queue size and active count converge to zero. Finally deinitialize and assert that `IsDispatchPaused()` is false.

  ```cpp
  TEST_ASSERT(scheduler.SetDispatchPaused(true) == 0,
              "scheduler accepts a dispatch pause while live");
  ReleaseBlockedUpload(state);
  TEST_ASSERT(!WaitForSendStart(state, 2, 200),
              "paused scheduler does not dispatch the next waiting upload");
  TEST_ASSERT(scheduler.SetDispatchPaused(false) == 0,
              "scheduler resumes dispatch");
  TEST_ASSERT(WaitForPathStart(state, TEXT("urgent-tail.txt")),
              "urgent upload remains first after resume");
  ```

- [x] **Step 2: Run the focused test and confirm the missing interface causes a compile failure**

  Run:

  ```powershell
  cmake --build _build --config Release --target NppFTP_FTPQueueTerminalLifecycle
  ```

  Expected: compile failure because `SetDispatchPaused` and `IsDispatchPaused` do not yet exist.

- [x] **Step 3: Add the monitor-protected pause state and gate dispatch**

  In `ConcurrentUploadScheduler.h`, declare the two public methods and add `m_dispatchPaused` beside the scheduler lifecycle booleans. In the constructor, `Initialize()`, and completed `Deinitialize()` cleanup, set it to `false` while holding the existing scheduler monitor.

  In `SetDispatchPaused`, reject a non-live scheduler, assign `m_dispatchPaused`, signal `m_monitor`, and return `0`. In `IsDispatchPaused`, take the monitor and return the stored value only while the scheduler is live.

  In `SchedulerLoop`, leave `FindIdleWorkerLocked()` and `m_policy.TakeNextMatching(...)` unchanged except for wrapping both dispatch decisions in this gate:

  ```cpp
  if (!m_dispatchPaused)
  {
      worker = FindIdleWorkerLocked();
      if (worker)
      {
          op = m_policy.TakeNextMatching(/* existing conflict predicate */);
          if (op)
          {
              worker->operations.push_back(op);
              worker->queue->AddQueueOp(op, false);
          }
      }
  }
  ```

  Do not block `AddQueueOp`, worker terminal processing, queue events, or teardown while paused.

- [x] **Step 4: Run the focused lifecycle test**

  Run:

  ```powershell
  cmake --build _build --config Release --target NppFTP_FTPQueueTerminalLifecycle
  & .\_build\tests\ftp_queue_terminal_lifecycle.exe
  ```

  Expected: `ftp_queue_terminal_lifecycle_exit=0`; the new test proves active work completes, waiting work does not dispatch until resume, and urgent ordering survives the pause.

- [ ] **Step 5: Commit the independently testable scheduler gate**

  ```powershell
  git add src/ConcurrentUploadScheduler.h src/ConcurrentUploadScheduler.cpp tests/ftp_queue_terminal_lifecycle.cpp
  git commit -m "feat: pause upload scheduler dispatch"
  ```

## Task 2: Upload-Only Single and Bulk Clear

**Files:**
- Modify: `src/ConcurrentUploadScheduler.h`
- Modify: `src/ConcurrentUploadScheduler.cpp`
- Modify: `src/FTPSession.h`
- Modify: `src/FTPSession.cpp`
- Test: `tests/ftp_queue_terminal_lifecycle.cpp`
- Test: `tests/ftp_session_upload_routing.ps1`

**Interfaces:**
- Produces `int ConcurrentUploadScheduler::CancelQueueOp(QueueOperation *op)` with selected active upload handling in its owning worker only.
- Produces `int ConcurrentUploadScheduler::CancelAllQueueOps()`.
- Produces `int FTPSession::SetUploadDispatchPaused(bool paused)` and `bool FTPSession::IsUploadDispatchPaused() const`.
- Produces `int FTPSession::ClearAllUploadOperations()`.
- `FTPSession::CancelOperation(QueueOperation *op)` continues routing upload operations to the scheduler and all other operations to `m_transferQueue`.

- [x] **Step 1: Write failing selected-active and clear-all lifecycle tests**

  Add two tests to `tests/ftp_queue_terminal_lifecycle.cpp`:

  1. `TestUploadSchedulerCancelsSelectedOperation()` starts two blocked uploads on two workers, calls `CancelQueueOp()` for only the first operation, and asserts exactly one wrapper receives `Abort()`, the selected operation reaches canceled terminal accounting once, and the other upload remains running until explicitly released.
  2. `TestUploadSchedulerClearsAllOperations()` starts one blocked upload, pauses dispatch, enqueues two waiting uploads, calls `CancelAllQueueOps()`, and asserts both waiting items reach canceled terminal accounting, the running upload is aborted, queue size reaches zero, and no `SendFile` starts after the scheduler is resumed.

  ```cpp
  TEST_ASSERT(scheduler.CancelQueueOp(firstActive) == 0,
              "selected active upload is aborted by its owning worker");
  TEST_ASSERT(state.firstWrapperAbortCount == 1,
              "clear selected does not abort the other worker");
  TEST_ASSERT(state.secondWrapperAbortCount == 0,
              "unselected active upload keeps running");

  TEST_ASSERT(scheduler.CancelAllQueueOps() == 0,
              "clear all accepts upload-only work");
  TEST_ASSERT(WaitForQueueIdle(scheduler),
              "clear all reaches terminal accounting for every upload");
  ```

- [x] **Step 2: Run the focused test and confirm the clear-all interface is absent**

  Run:

  ```powershell
  cmake --build _build --config Release --target NppFTP_FTPQueueTerminalLifecycle
  ```

  Expected: compile failure because `CancelAllQueueOps` does not exist.

- [x] **Step 3: Make `CancelQueueOp` abort only the selected active upload**

  Retain the existing pending-add and waiting-policy removal paths. In the worker-owned path, first call the existing `worker->queue->CancelQueueOp(op, &terminal, false)`.

  - When it returns `0`, erase the operation from `worker->operations`, run the existing terminal follow-up outside the scheduler monitor, and return `0`.
  - When it returns `-1` and `op->GetRunning()` is true, call only `worker->queue->AbortActive()`. Return `0` when that worker accepts the abort; return `-1` only when it cannot abort the selected operation.
  - Do not call `ConcurrentUploadScheduler::AbortActive()` from this selected-item path, because that method intentionally aborts every active upload worker.

  Keep the worker operation in `worker->operations` until its normal terminal callback removes it. This prevents the terminal callback from seeing an ownership gap.

- [x] **Step 4: Implement upload-only `CancelAllQueueOps` and session delegation**

  In `CancelAllQueueOps`, take `m_monitor`, reject an unavailable scheduler, mark each `m_pendingAdds` entry canceled, and remove every waiting operation from `m_policy` into a local vector. While the same monitor remains held, iterate each worker's visible operations: cancel waiting worker-queue items and erase their scheduler ownership immediately; invoke that worker queue's `AbortActive()` for an active item but leave its ownership to the normal terminal callback. Collect terminal follow-ups and release the monitor before deleting or posting them.

  For each pending add, leave its existing Add acknowledgement path to issue its canceled terminal cleanup. For each waiting-policy item, send the existing remove notification and run its current terminal follow-up exactly once. Do not snapshot worker pointers across an unlocked period: a terminal callback may otherwise remove and delete an operation before clear-all reaches it. Return `0` if the scheduler accepted the command, even when the queue was already empty.

  In `FTPSession`, delegate pause, state query, and clear-all calls to `m_uploadScheduler`, returning `-1`/`false` when no scheduler exists:

  ```cpp
  int FTPSession::ClearAllUploadOperations()
  {
      return m_uploadScheduler ? m_uploadScheduler->CancelAllQueueOps() : -1;
  }
  ```

- [x] **Step 5: Add static routing assertions**

  Extend `tests/ftp_session_upload_routing.ps1` to assert that:

  ```powershell
  Assert-Contains $sessionCpp 'm_uploadScheduler->CancelQueueOp(cancelOp)'
  Assert-Contains $sessionCpp 'ClearAllUploadOperations'
  Assert-Contains $schedulerCpp 'worker->queue->AbortActive()'
  Assert-NotContains $sessionCpp 'ClearAllUploadOperations()' 'm_transferQueue->'
  ```

  The existing assertions for upload routing and priority remain enabled.

- [x] **Step 6: Run all focused queue and routing checks**

  Run:

  ```powershell
  cmake --build _build --config Release --target NppFTP_FTPQueueTerminalLifecycle
  & .\_build\tests\ftp_queue_terminal_lifecycle.exe
  pwsh -NoProfile -File .\tests\ftp_session_upload_routing.ps1
  ```

  Expected: `ftp_queue_terminal_lifecycle_exit=0` and `ftp_session_upload_routing_exit=0`.

- [ ] **Step 7: Commit upload-only cancellation behavior**

  ```powershell
  git add src/ConcurrentUploadScheduler.h src/ConcurrentUploadScheduler.cpp src/FTPSession.h src/FTPSession.cpp tests/ftp_queue_terminal_lifecycle.cpp tests/ftp_session_upload_routing.ps1
  git commit -m "feat: add upload queue clear controls"
  ```

## Task 3: Transfer-List Context Menu and Documentation

**Files:**
- Modify: `src/Windows/Commands.h`
- Modify: `src/Windows/FTPWindow.h`
- Modify: `src/Windows/FTPWindow.cpp`
- Modify: `tests/ftp_session_upload_routing.ps1`
- Modify: `todo.md`
- Modify: `README.md`
- Modify: `history.md`

**Interfaces:**
- Consumes `FTPSession::SetUploadDispatchPaused`, `FTPSession::IsUploadDispatchPaused`, `FTPSession::ClearAllUploadOperations`, and upload-aware `FTPSession::CancelOperation` from Tasks 1 and 2.
- Produces `IDM_POPUP_QUEUE_TOGGLE_UPLOAD_PAUSE`, `IDM_POPUP_QUEUE_CLEAR_SELECTED_UPLOAD`, and `IDM_POPUP_QUEUE_CLEAR_ALL_UPLOADS`.
- Produces upload-only popup menus for a selected upload row and for empty transfer-list space.

- [x] **Step 1: Write failing static UI contract assertions**

  Extend `tests/ftp_session_upload_routing.ps1` with assertions for the three command names, `QueueTypeUpload` menu selection, `IsUploadDispatchPaused()`, `SetUploadDispatchPaused(...)`, `ClearAllUploadOperations()`, and a selected-item call to `CancelOperation(m_cancelOperation)`. Also assert that `IDM_POPUP_QUEUE_ABORT` still calls `AbortTransfer()` so the toolbar/legacy generic action keeps its all-transfer meaning.

  ```powershell
  Assert-Contains $ftpWindowCpp 'QueueTypeUpload'
  Assert-Contains $ftpWindowCpp 'IDM_POPUP_QUEUE_TOGGLE_UPLOAD_PAUSE'
  Assert-Contains $ftpWindowCpp 'm_ftpSession->SetUploadDispatchPaused'
  Assert-Contains $ftpWindowCpp 'm_ftpSession->ClearAllUploadOperations()'
  Assert-Contains $ftpWindowCpp 'm_ftpSession->CancelOperation(m_cancelOperation)'
  ```

- [x] **Step 2: Run the routing script and confirm it fails before the UI is wired**

  Run:

  ```powershell
  pwsh -NoProfile -File .\tests\ftp_session_upload_routing.ps1
  ```

  Expected: nonzero exit with the first missing upload context-menu contract.

- [x] **Step 3: Add dedicated command identifiers and popup menus**

  Add the three identifiers after the existing remote-browser popup identifiers in `Commands.h` without renumbering existing commands. In `FTPWindow.h`, add dedicated upload popup handles and initialize/destroy them beside `m_popupQueueActive` and `m_popupQueueHold`.

  In `CreateMenus`, create an upload-row popup containing:

  ```cpp
  AppendMenu(m_popupQueueUpload, MF_STRING, IDM_POPUP_QUEUE_TOGGLE_UPLOAD_PAUSE,
             TEXT("Pause all uploads"));
  AppendMenu(m_popupQueueUpload, MF_SEPARATOR, 0, NULL);
  AppendMenu(m_popupQueueUpload, MF_STRING, IDM_POPUP_QUEUE_CLEAR_SELECTED_UPLOAD,
             TEXT("Clear selected upload task"));
  AppendMenu(m_popupQueueUpload, MF_STRING, IDM_POPUP_QUEUE_CLEAR_ALL_UPLOADS,
             TEXT("Clear all upload tasks"));
  ```

  Create a second empty-area popup containing the toggle and `Clear all upload tasks`, with no selected-item command.

- [x] **Step 4: Choose the upload menu only for upload rows or empty transfer space**

  In the transfer-list right-click handling, clear `m_cancelOperation` first. If a selected operation exists and `op->GetType() == QueueTypeUpload`, store it and show the upload-row popup. If no operation is selected, show the empty-area upload popup. All non-upload rows continue selecting the pre-existing active/held popups.

  Before displaying either upload popup, update the toggle item text according to `m_ftpSession->IsUploadDispatchPaused()`:

  ```cpp
  ModifyMenu(m_popupQueueUpload, IDM_POPUP_QUEUE_TOGGLE_UPLOAD_PAUSE,
             MF_BYCOMMAND | MF_STRING, IDM_POPUP_QUEUE_TOGGLE_UPLOAD_PAUSE,
             paused ? TEXT("Resume uploads") : TEXT("Pause all uploads"));
  ```

  Use the same replacement text on the empty-area upload popup. The commands remain mutually exclusive because one menu item changes label rather than adding two choices.

- [x] **Step 5: Execute commands and write non-blocking Output status**

  In the command switch:

  ```cpp
  case IDM_POPUP_QUEUE_TOGGLE_UPLOAD_PAUSE:
      if (m_ftpSession && m_ftpSession->SetUploadDispatchPaused(
              !m_ftpSession->IsUploadDispatchPaused()) == 0)
      {
          PrintMessage(m_ftpSession->IsUploadDispatchPaused()
              ? TEXT("Uploads paused; active uploads will finish.")
              : TEXT("Uploads resumed."));
      }
      break;

  case IDM_POPUP_QUEUE_CLEAR_SELECTED_UPLOAD:
      if (m_cancelOperation && m_cancelOperation->GetType() == QueueTypeUpload)
          m_ftpSession->CancelOperation(m_cancelOperation);
      m_cancelOperation = NULL;
      break;

  case IDM_POPUP_QUEUE_CLEAR_ALL_UPLOADS:
      m_ftpSession->ClearAllUploadOperations();
      m_cancelOperation = NULL;
      break;
  ```

  Use the existing FTPWindow Output function already used for queue events; do not show a modal dialog. When disconnecting, destroy/clear popup state with the rest of the session state so a stale selected operation cannot be reused.

- [x] **Step 6: Update task documentation and manual acceptance list**

  In `todo.md`, mark every 5.2 implementation item completed and retain a manual QA subsection for: waiting upload pause/resume, active upload continues during pause, resume urgent-before-normal, selected waiting clear, selected active clear, clear-all with recursive upload, disconnect/reconnect reset, and proof that download/mutation rows retain their old context menu.

  In `README.md`, add the queue-control behavior to the feature list and list the same manual protocol QA boundary. In `history.md`, record the exact pause semantics, upload-only scope, focused test commands/results, build result, and external live FTP/FTPS/SFTP verification boundary.

- [x] **Step 7: Run the complete focused suite, build, and whitespace check**

  Run:

  ```powershell
  pwsh -NoProfile -File .\tests\ftp_session_upload_routing.ps1
  & .\_build\tests\ftp_queue_terminal_lifecycle.exe
  .\build.bat -Arch x64 -Config Release
  git -c core.whitespace=cr-at-eol diff --check
  ```

  Expected: routing script reports `ftp_session_upload_routing_exit=0`; lifecycle executable reports `ftp_queue_terminal_lifecycle_exit=0`; build produces `_build\Release\NppFTP.dll` and the x64 ZIP; whitespace check emits no errors.

- [ ] **Step 8: Commit the UI and documented verification boundary**

  ```powershell
  git add src/Windows/Commands.h src/Windows/FTPWindow.h src/Windows/FTPWindow.cpp tests/ftp_session_upload_routing.ps1 todo.md README.md history.md
  git commit -m "feat: control bulk upload queue"
  ```

## Plan Self-Review

### Spec coverage

- Pause/resume has one mutually exclusive action on upload rows and empty transfer-list space: Task 3.
- Pause protects active transfers and retains waiting priority/conflict rules: Task 1.
- Selected upload clear handles waiting and active work without global abort: Task 2 and Task 3.
- Clear-all only affects uploads, including waiting and active work: Task 2 and Task 3.
- Recursive batch accounting is protected by existing terminal paths and covered in Task 2 lifecycle verification.
- Output feedback, disconnect reset, and manual FTP/FTPS/SFTP QA boundary are in Task 3.
- Downloads, browse, and mutation queues retain their behavior because all new session APIs delegate only to `m_uploadScheduler`: Task 2 and Task 3.

### Type consistency

`FTPSession` consumes `ConcurrentUploadScheduler::SetDispatchPaused(bool)`, `IsDispatchPaused() const`, and `CancelAllQueueOps()` exactly as defined in Task 1 and Task 2. `FTPWindow` consumes the public `FTPSession` façade rather than accessing scheduler internals.
