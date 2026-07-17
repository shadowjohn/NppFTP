#include "src/RecentDirs.h"

#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>

int main()
{
	std::vector<std::string> dirs;
	assert(recent_dirs_add(dirs, "/var/www", 8) == 0);
	assert(recent_dirs_add(dirs, "/var/log", 8) == 0);
	assert(recent_dirs_add(dirs, "/var/www", 8) == 0);
	assert(dirs.size() == 2);
	assert(dirs[0] == "/var/www");
	assert(dirs[1] == "/var/log");

	for (int i = 0; i < 10; i++) {
		char path[16]{};
		sprintf_s(path, "/dir%d", i);
		assert(recent_dirs_add(dirs, path, 8) == 0);
	}
	assert(dirs.size() == 8);
	assert(dirs[0] == "/dir9");
	assert(dirs[7] == "/dir2");
	assert(recent_dirs_add(dirs, NULL, 8) == -1);
	assert(recent_dirs_add(dirs, "", 8) == -1);
	assert(recent_dirs_matches("/var/www/html", "/var/w"));
	assert(recent_dirs_matches("/var/www/html", ""));
	assert(!recent_dirs_matches("/var/www/html", "/tmp"));

	printf("recent_dirs_exit=0\n");
	return 0;
}
