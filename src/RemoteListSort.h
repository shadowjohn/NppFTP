#ifndef REMOTE_LIST_SORT_H
#define REMOTE_LIST_SORT_H

#include <windows.h>
#include <shlwapi.h>
#include <string.h>
#include <tchar.h>

enum RemoteListSortColumn {
	RemoteSortNone = -1,
	RemoteSortName = 0,
	RemoteSortSize,
	RemoteSortModified,
	RemoteSortType,
	RemoteSortPermissions
};

struct RemoteListSortItem {
	void *data;
	bool isDirectory;
	bool isLink;
	LONGLONG size;
	FILETIME modified;
	const TCHAR *name;
	const char *path;
	const char *permissions;
};

static inline int remote_list_compare_text(const TCHAR *left, const TCHAR *right)
{
	left = left ? left : TEXT("");
	right = right ? right : TEXT("");
	int result = CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, left, -1, right, -1);
	return result ? result - CSTR_EQUAL : lstrcmpi(left, right);
}

static inline const TCHAR *remote_list_type_value(const RemoteListSortItem &item)
{
	if (item.isDirectory)
		return TEXT("");
	const TCHAR *extension = item.name ? PathFindExtension(item.name) : NULL;
	if (extension && extension[0] == TEXT('.') && extension[1] && extension != item.name)
		return extension + 1;
	return item.isLink ? TEXT("LINK") : TEXT("");
}

static inline bool remote_list_sort_less(const RemoteListSortItem &left,
	const RemoteListSortItem &right, int column, bool ascending)
{
	if (left.isDirectory != right.isDirectory)
		return left.isDirectory;

	int result = 0;
	switch (column) {
		case RemoteSortSize:
			if (!left.isDirectory)
				result = left.size < right.size ? -1 : (left.size > right.size ? 1 : 0);
			break;
		case RemoteSortModified:
			result = CompareFileTime(&left.modified, &right.modified);
			break;
		case RemoteSortType:
			result = remote_list_compare_text(remote_list_type_value(left), remote_list_type_value(right));
			break;
		case RemoteSortPermissions:
			result = lstrcmpiA(left.permissions ? left.permissions : "", right.permissions ? right.permissions : "");
			break;
		case RemoteSortName:
		default:
			result = remote_list_compare_text(left.name, right.name);
			break;
	}

	if (result == 0)
		result = remote_list_compare_text(left.name, right.name);
	if (result == 0)
		result = strcmp(left.path ? left.path : "", right.path ? right.path : "");
	return ascending ? result < 0 : result > 0;
}

#endif //REMOTE_LIST_SORT_H
