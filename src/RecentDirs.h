#ifndef RECENTDIRS_H
#define RECENTDIRS_H

#include <algorithm>
#include <string>
#include <vector>

static inline int recent_dirs_add(std::vector<std::string> & dirs, const char * path, size_t maxCount)
{
	if (!path || path[0] == 0 || maxCount == 0)
		return -1;
	std::vector<std::string>::iterator existing = std::find(dirs.begin(), dirs.end(), path);
	if (existing != dirs.end())
		dirs.erase(existing);
	dirs.insert(dirs.begin(), path);
	if (dirs.size() > maxCount)
		dirs.resize(maxCount);
	return 0;
}

static inline bool recent_dirs_matches(const char * path, const char * prefix)
{
	if (!path || !prefix)
		return false;
	while (*prefix) {
		if (*path++ != *prefix++)
			return false;
	}
	return true;
}

#endif //RECENTDIRS_H
