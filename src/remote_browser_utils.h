#ifndef REMOTE_BROWSER_UTILS_H
#define REMOTE_BROWSER_UTILS_H

#include <shlwapi.h>
#include <tchar.h>

static inline bool remote_browser_matches_filter(const TCHAR *name, const TCHAR *filter)
{
	if (!name)
		return false;
	if (!filter || filter[0] == 0)
		return true;
	return StrStrI(name, filter) != NULL;
}

#endif //REMOTE_BROWSER_UTILS_H
