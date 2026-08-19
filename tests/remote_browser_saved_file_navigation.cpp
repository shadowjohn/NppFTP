#include "src/remote_browser_utils.h"

#include <stdio.h>
#include <string.h>

static int Check(bool condition, const char * message)
{
	if (condition)
		return 0;
	printf("remote_browser_saved_file_navigation_failure=%s\n", message);
	return 1;
}

int main()
{
	char parent[MAX_PATH]{};
	if (Check(remote_browser_saved_file_parent_path("/var/www/html/3waAIHub/index.php", parent, MAX_PATH) == 0, "nested saved file parent resolves")) return 1;
	if (Check(strcmp(parent, "/var/www/html/3waAIHub") == 0, "nested saved file parent value")) return 1;

	if (Check(remote_browser_saved_file_parent_path("/index.php", parent, MAX_PATH) == 0, "root saved file parent resolves")) return 1;
	if (Check(strcmp(parent, "/") == 0, "root saved file parent value")) return 1;

	if (Check(remote_browser_saved_file_parent_path("/var/www/html/3waAIHub/", parent, MAX_PATH) == -1, "directory path is not a saved file")) return 1;
	if (Check(remote_browser_saved_file_parent_path("index.php", parent, MAX_PATH) == -1, "relative saved file path is rejected")) return 1;

	printf("remote_browser_saved_file_navigation_exit=0\n");
	return 0;
}
