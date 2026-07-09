#include "src/remote_browser_utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
	assert(remote_browser_matches_filter(TEXT("index.html"), TEXT("")));
	assert(remote_browser_matches_filter(TEXT("index.html"), NULL));
	assert(remote_browser_matches_filter(TEXT("index.html"), TEXT("INDEX")));
	assert(remote_browser_matches_filter(TEXT("folder"), TEXT("old")));
	assert(!remote_browser_matches_filter(TEXT("folder"), TEXT("zip")));
	assert(!remote_browser_matches_filter(NULL, TEXT("zip")));

	TCHAR text[64]{};
	remote_browser_format_size(false, 1536, text, 64);
	assert(_tcscmp(text, TEXT("1536")) == 0);
	remote_browser_format_size(true, 1536, text, 64);
	assert(_tcscmp(text, TEXT("")) == 0);
	remote_browser_format_size(false, -1, text, 64);
	assert(_tcscmp(text, TEXT("")) == 0);

	remote_browser_type_text(false, false, TEXT("fzers.zip"), text, 64);
	assert(_tcscmp(text, TEXT("ZIP")) == 0);
	remote_browser_type_text(true, false, TEXT("docs"), text, 64);
	assert(_tcscmp(text, TEXT("")) == 0);
	remote_browser_type_text(false, true, TEXT("shortcut"), text, 64);
	assert(_tcscmp(text, TEXT("LINK")) == 0);

	remote_browser_permission_text("drwxr-xr-x", text, 64);
	assert(_tcscmp(text, TEXT("drwxr-xr-x")) == 0);
	remote_browser_permission_text(NULL, text, 64);
	assert(_tcscmp(text, TEXT("")) == 0);

	char path[64]{};
	assert(remote_browser_simplify_typed_path("/abs/path", "/var/www/html", path, 64) == 0);
	assert(strcmp(path, "/abs/path") == 0);
	assert(remote_browser_simplify_typed_path("child", "/var/www/html", path, 64) == 0);
	assert(strcmp(path, "/var/www/html/child") == 0);
	assert(remote_browser_simplify_typed_path("../sibling", "/var/www/html", path, 64) == 0);
	assert(strcmp(path, "/var/www/sibling") == 0);
	assert(remote_browser_simplify_typed_path("/too/long/path", "/", path, 8) == -1);

	printf("remote_browser_utils_exit=0\n");
	return 0;
}
