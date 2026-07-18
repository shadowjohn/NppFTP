#include "src/RemoteListSort.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <vector>

static RemoteListSortItem item(void *data, bool dir, LONGLONG size, DWORD modified,
	const TCHAR *name, const char *path, const char *permissions)
{
	RemoteListSortItem value{};
	value.data = data;
	value.isDirectory = dir;
	value.isLink = false;
	value.size = size;
	value.modified.dwLowDateTime = modified;
	value.name = name;
	value.path = path;
	value.permissions = permissions;
	return value;
}

int main()
{
	RemoteListSortItem dirA = item((void*)1, true, 0, 20, TEXT("\u76EE\u9304A"), "/\u76EE\u9304A", "drwxr-xr-x");
	RemoteListSortItem dirB = item((void*)2, true, 0, 10, TEXT("\u76EE\u9304B"), "/\u76EE\u9304B", "drwx------");
	RemoteListSortItem smallFile = item((void*)3, false, 100, 30, TEXT("a.txt"), "/a.txt", "-rw-r--r--");
	RemoteListSortItem large = item((void*)4, false, 5368709120LL, 40, TEXT("b.zip"), "/b.zip", "-rw-------");

	assert(remote_list_sort_less(dirA, smallFile, RemoteSortName, false));
	assert(!remote_list_sort_less(smallFile, dirA, RemoteSortName, false));
	assert(remote_list_sort_less(smallFile, large, RemoteSortSize, true));
	assert(remote_list_sort_less(large, smallFile, RemoteSortSize, false));
	assert(remote_list_sort_less(dirB, dirA, RemoteSortModified, true));
	bool chineseForward = remote_list_sort_less(dirA, dirB, RemoteSortName, true);
	bool chineseReverse = remote_list_sort_less(dirB, dirA, RemoteSortName, true);
	assert(chineseForward != chineseReverse);
	assert(remote_list_sort_less(smallFile, large, RemoteSortType, true));
	assert(remote_list_sort_less(large, smallFile, RemoteSortPermissions, true) !=
		remote_list_sort_less(smallFile, large, RemoteSortPermissions, true));

	RemoteListSortItem sameNameA = item((void*)5, false, 1, 1, TEXT("Readme"), "/A/Readme", "");
	RemoteListSortItem sameNameB = item((void*)6, false, 1, 1, TEXT("README"), "/B/README", "");
	assert(remote_list_sort_less(sameNameA, sameNameB, RemoteSortName, true));
	assert(!remote_list_sort_less(sameNameB, sameNameA, RemoteSortName, true));

	printf("remote_list_sort_exit=0\n");
	return 0;
}
