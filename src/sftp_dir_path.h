/*
    SFTP directory listing path composition helper.
    Extracted so it can be unit-tested without a live SFTP session.
*/

#ifndef SFTP_DIR_PATH_H
#define SFTP_DIR_PATH_H

#include <stdio.h>
#include <string.h>

/*
    Compose a directory entry's full path from a parent directory and a
    filename returned by sftp_readdir.

    Returns 0 on success, -1 if the composed path would exceed buf_size
    (in which case buf contents are undefined and the caller should skip
    the entry).
*/
static inline int sftp_compose_entry_path(
    const char *dir_path,
    const char *entry_name,
    char *buf,
    size_t buf_size)
{
    if (!dir_path || !entry_name || !buf || buf_size == 0)
        return -1;

    size_t dlen = strlen(dir_path);
    int needs_slash = (dlen > 0) && (dir_path[dlen - 1] != '/');

    int written = snprintf(buf, buf_size,
        "%s%s%s", dir_path, needs_slash ? "/" : "", entry_name);

    if (written < 0 || (size_t)written >= buf_size)
        return -1;

    return 0;
}

#endif /* SFTP_DIR_PATH_H */
