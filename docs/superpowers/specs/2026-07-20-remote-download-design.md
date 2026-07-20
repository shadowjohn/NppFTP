# Remote Download Design

## Goal

Make `Download...` a user-directed local save action. `Edit` remains the only
action that downloads into the NppFTP cache and opens the result in Notepad++.

## User Flow

- The green toolbar Download button and flat remote-list file/directory context
  menus use one `PromptRemoteDownload(FileObject*)` path.
- For a file, show the existing native Save As dialog with the remote filename
  prefilled. Queue one download to the selected filename and do not prompt to
  open it afterward.
- For a directory, show the existing native folder picker. If the user chooses
  `D:\\Backup` for remote `/var/www/html`, download into
  `D:\\Backup\\html\\...`. Existing local directories merge; the selected
  directory itself is never replaced.
- Cancelling either dialog does nothing. The Edit menu item keeps its current
  cache-download-and-open behavior.

## Batch Design

- Add a small `RemoteDownloadPlan`, parallel to the existing upload plan. It
  records remote directories/files and their validated local destination paths.
- Add one main-queue scan operation that recursively calls `GetDir` for the
  selected directory before any transfer starts. It discovers every descendant
  so collapsed or unvisited remote directories cannot be skipped.
- Local paths are formed only from validated single remote path segments and
  must stay beneath the chosen root directory. Empty, dot, dot-dot, invalid,
  or overlong local paths are rejected and recorded as failures.
- Remote symlinks are not traversed. They are logged as skipped items so a
  server-side loop cannot make the download run forever or leave the chosen
  local root.
- After the scan returns to the UI thread, create required local directories
  parent-first. A local directory where a remote file belongs, or a local file
  where a remote directory belongs, is a failed item and is not overwritten.
- Existing local files use an overwrite dialog with Overwrite, Skip, Cancel,
  Overwrite all, and Skip all. Only selected files are queued.
- Reuse `QueueDownload` transfer behavior. A thin batch-owned download queue
  operation and one completion marker keep the plan alive through disconnect,
  cancellation, and every queued file. Queue rows provide per-file progress.

## Completion And Errors

- A normal Save As download logs success and never opens Notepad++ afterward.
- A directory download records each failed file/directory in Output and shows
  one final summary dialog with completed and failed counts.
- A failed standalone file download uses the existing single-operation failure
  message. Folder scan failures and local-root creation failures show one
  actionable error before transfers begin.
- Download batch overwrite state belongs to that batch only; it does not alter
  upload overwrite state or later sessions.

## Scope And Checks

- Add `Download...` to the flat remote file and directory menus; the toolbar
  uses the same handler for either selected item.
- Change the legacy Download command to the same user-directed behavior so it
  cannot silently route a file to cache. Leave `Edit` unchanged.
- Add a focused `RemoteDownloadPlan` test for root construction, nested paths,
  path rejection, local collisions, and symlink skipping.
- Manual QA: Save As cancel/success/no-open; directory root creation; nested
  files; overwrite/skip/all/cancel; file-vs-directory collisions; disconnected
  scan; FTP, FTPS, and SFTP transfers; queue progress and one final summary.

## Out Of Scope

- Opening Explorer after completion, resume support, timestamp/permission
  preservation, and traversing remote symlinks.
