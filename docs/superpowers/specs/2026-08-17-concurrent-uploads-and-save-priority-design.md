# Concurrent Uploads And Save Priority Design

## Goal

Allow one NppFTP session to upload several files concurrently while keeping
remote-file saves responsive. The global upload limit is configurable from 1
to 8 and defaults to 1, preserving existing behavior for every user who does
not change it.

## Scope

- The limit is global, not profile-specific, and applies to FTP, FTPS, and
  SFTP uploads.
- Only uploads become concurrent. Remote browsing, directory scans, download,
  copy, and mutation operations retain their current single-operation flow.
- The setting takes effect for the next NppFTP connection. An active session
  keeps its established worker count until it disconnects.
- The Global settings dialog's fixed strings, including the new upload-limit
  field, use Traditional Chinese. This is a scoped settings-dialog update, not
  a language-selection system or a translation of the remaining plugin UI.
- Localization is otherwise intentionally deferred to a later slice.

## Upload Worker Design

- Add a dedicated upload scheduler with one shared pending queue and 1-8
  workers. Each worker owns an independently connected `FTPClientWrapper`
  created through the existing `Clone()` API; no protocol connection is shared
  by multiple threads.
- Add `同時上傳數量` to the Global settings dialog. Persist it in the existing
  NppFTP settings XML as an integer, clamp malformed values to 1, and constrain
  UI input to 1 through 8. Translate the dialog's existing fixed labels and OK
  button to Traditional Chinese in the same change.
- The existing serial transfer queue remains responsible for downloads and
  other non-upload transfer operations. The scheduler receives only upload
  file operations, so enabling parallel uploads does not silently enable
  parallel downloads or remote mutations.
- A worker connects lazily before its first operation. A connection or upload
  failure is reported for that operation and must not stop other workers or
  discard other pending uploads.
- Session shutdown stops dispatching, aborts all active upload workers, waits
  for their queues to stop, then disconnects and destroys every cloned wrapper.
- The toolbar Abort command aborts every active upload worker. The queue
  context-menu Abort command continues to target only the selected active
  operation. Pending-item removal continues to remove only that item.

## Recursive Upload Batches

- Remote directories remain prepared serially and parent-first before the
  batch's files enter the parallel upload scheduler. This preserves the
  existing safe directory-merge behavior.
- A recursive upload batch counts every selected file until it reaches a
  terminal state: completed, failed, or canceled. Its summary is emitted once,
  only after the count reaches zero; a queue marker must never overtake files
  that are active on another worker.
- Per-file queue rows and progress remain visible. One failed file records its
  failure and does not stop the remaining files in the same batch.

## Save Priority

- Only the automatic path `NPPN_FILESAVED -> NppFTP::OnSave ->
  FTPSession::UploadFileCache` is urgent. Toolbar uploads, picker uploads,
  drag/drop uploads, and recursive batch files remain normal priority.
- Urgent saves are inserted ahead of all waiting normal uploads. They never
  interrupt a running upload; the next worker that becomes free takes the
  urgent save first.
- If the same local/remote file is already waiting, promote that pending item
  instead of adding a duplicate. It reads the latest local file content when
  it starts.
- If an older version of that file is already running, queue one urgent follow-
  up upload. It runs after the active operation and leaves the remote file with
  the latest saved content.

## Verification

- Add a focused assert-based scheduler test covering the 1-worker FIFO case,
  2- and 8-worker dispatch limits, urgent-before-normal ordering, pending-save
  promotion, active-save follow-up, cancellation, and batch completion only
  after all selected files become terminal.
- Build with `build.bat -Arch x64 -Config Release` and keep `git diff --check`
  clean.
- Manual QA against FTP, FTPS, and SFTP: default 1 worker; configured 2 and 8
  workers; mixed successful and failed files; recursive directory creation;
  toolbar Abort-all; queue Abort-one; save while normal uploads are active;
  disconnect while work is pending; and reconnect after changing the setting.

## Out Of Scope

- Per-profile concurrency limits, parallel downloads, bandwidth throttling,
  automatic retries, resuming partial uploads, and any localization work.
