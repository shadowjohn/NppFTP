#include "src/sftp_dir_path.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

/* MAX_PATH is normally from <windows.h>; define it for this standalone test. */
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

int main()
{
	char buf[MAX_PATH + 1];

	/* --- normal cases --- */

	/* directory with trailing slash */
	assert(sftp_compose_entry_path("/home/user/", "file.txt", buf, sizeof(buf)) == 0);
	assert(strcmp(buf, "/home/user/file.txt") == 0);

	/* directory without trailing slash */
	assert(sftp_compose_entry_path("/home/user", "file.txt", buf, sizeof(buf)) == 0);
	assert(strcmp(buf, "/home/user/file.txt") == 0);

	/* root directory */
	assert(sftp_compose_entry_path("/", "file.txt", buf, sizeof(buf)) == 0);
	assert(strcmp(buf, "/file.txt") == 0);

	/* --- overflow cases --- */

	/* filename alone exceeds buffer */
	{
		char longname[512];
		memset(longname, 'A', sizeof(longname) - 1);
		longname[sizeof(longname) - 1] = '\0';
		assert(sftp_compose_entry_path("/tmp", longname, buf, sizeof(buf)) == -1);
	}

	/* path + filename together exceed buffer */
	{
		char longdir[200];
		memset(longdir, 'D', sizeof(longdir) - 1);
		longdir[0] = '/';
		longdir[sizeof(longdir) - 1] = '\0';

		char longname[200];
		memset(longname, 'F', sizeof(longname) - 1);
		longname[sizeof(longname) - 1] = '\0';

		assert(sftp_compose_entry_path(longdir, longname, buf, sizeof(buf)) == -1);
	}

	/* exactly at the boundary: path + '/' + name == MAX_PATH should succeed */
	{
		/* buf is MAX_PATH+1 = 261 bytes, so total string can be MAX_PATH=260 chars + NUL */
		/* path="/xxx" (4 bytes) + "/" (1 byte) + name (255 bytes) = 260, fits */
		char name255[256];
		memset(name255, 'N', 255);
		name255[255] = '\0';
		assert(sftp_compose_entry_path("/xxx", name255, buf, sizeof(buf)) == 0);
		assert(strlen(buf) == 260);
	}

	/* one byte over: path="/xxx" + "/" + name (256 bytes) = 261, needs 262 with NUL, fails */
	{
		char name256[257];
		memset(name256, 'N', 256);
		name256[256] = '\0';
		assert(sftp_compose_entry_path("/xxx", name256, buf, sizeof(buf)) == -1);
	}

	/* --- edge cases --- */

	/* empty path */
	assert(sftp_compose_entry_path("", "file.txt", buf, sizeof(buf)) == 0);
	assert(strcmp(buf, "file.txt") == 0);

	/* NULL inputs */
	assert(sftp_compose_entry_path(NULL, "file.txt", buf, sizeof(buf)) == -1);
	assert(sftp_compose_entry_path("/tmp", NULL, buf, sizeof(buf)) == -1);
	assert(sftp_compose_entry_path("/tmp", "f", NULL, sizeof(buf)) == -1);
	assert(sftp_compose_entry_path("/tmp", "f", buf, 0) == -1);

	printf("sftp_dir_path_exit=0\n");
	return 0;
}
