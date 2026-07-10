# Remote File Operations Design

## Goal

Make the PSPad-like flat remote browser usable for normal file management:
upload by file picker or drag and drop, edit, chmod, create, rename, delete,
and recursive directory upload with visible progress.

## Scope

- Keep all newly introduced UI text in English.
- A separate localization slice will add language selection with Traditional
  Chinese as the default.
- Reuse `FTPWindow`, `FileObject`, `FTPSession`, and the existing queue
  operations. Do not introduce a second transfer system or a UI library.

## Flat List Interaction

| Target | Context menu | Keyboard behavior |
| --- | --- | --- |
| Directory | `Upload files...`, `CHMOD...`, `Delete directory`, `Rename` | `F2` renames |
| File | `Edit`, `CHMOD...`, `Delete file`, `Rename` | `F2` renames; Edit matches double-click |
| Empty list area | `Refresh`, `Create directory...`, `New file...`, `Upload files...` | Operates on the current directory |
| `..` | No destructive commands | Double-click returns to the parent |

Right-click first selects the item under the pointer. Keyboard menu and F2
operate on the current selected item. Existing confirmation dialogs and
existing FTP queue methods remain the authority for delete, rename, create,
and edit/download actions.

## Upload Behavior

### Target Resolution

- Dropping on empty list space uploads to the current directory.
- Dropping on a file uploads to the current directory.
- Dropping on a directory uploads into that directory.
- The file picker accepts multiple selections and follows the same path.

### Files and Overwrites

For a direct file upload, the target directory's current cache determines
whether the remote file is known to exist. A known collision shows `Overwrite`
and `Skip`.
Choosing Skip skips only that file. The dialog includes `Don't ask again this
session (overwrite all)`; it takes effect only when the user selects
Overwrite, and resets when the FTP session disconnects.

The normal direct-file path does not add a network preflight. Refreshing first
is the user-controlled way to refresh that cache.

### Directories

Dropping or selecting a local directory recursively uploads its contents. The
remote directory named after the local directory is created beneath the target.
If it already exists, the upload merges into it; remote contents are never
deleted to recreate a directory.

Recursive upload needs a targeted remote scan for the directories it will
enter. This is required to identify descendant file collisions before work is
queued, so overwrite prompts remain on the UI thread. The scan is only for
directory uploads; it does not change the direct-file behavior above.

The resulting directory-create and upload actions run in one ordered transfer
queue: each parent directory is ensured before its children and files. The
queue window shows progress for every file upload, not only the first row.

## CHMOD

Replace the numeric-only prompt with a native dialog containing Owner, Group,
and Other read/write/execute checkboxes. The nine checkboxes and the three
digit octal value stay synchronized. Confirming submits the three-digit mode
through the existing `FTPSession::Chmod` path.

## Errors and Refresh

Successful mutations refresh the affected directory. A failed single action
(chmod, rename, delete, create, or direct upload) writes the full detail to
Output and shows a result dialog. Recursive uploads write each failed path to
Output and show one completion summary rather than a modal dialog per file.

SFTP exposes distinct permission and missing-path responses, which are shown
as such. FTP and FTPS frequently return an ambiguous server rejection (often
`550`); in that case the UI says that the server rejected the operation and
does not falsely claim a specific cause.

## Validation

- Add small test-first checks for pure target selection, overwrite-policy, and
  chmod octal/checkbox conversion logic.
- Build with `build.bat`.
- Manually verify the three drag targets, multi-file picker, overwrite and
  session reset, merge upload, progress per file, all context menus, F2,
  recursive error summary, and SFTP/FTP failure wording.

## Non-Goals

- No recursive delete, sync mode, or upload resume.
- No persistence for the session-only overwrite choice.
- No localization in this slice.
