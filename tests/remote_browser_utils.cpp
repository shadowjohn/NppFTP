#include "src/remote_browser_utils.h"

#include <assert.h>
#include <stdio.h>

int main()
{
	assert(remote_browser_matches_filter(TEXT("index.html"), TEXT("")));
	assert(remote_browser_matches_filter(TEXT("index.html"), NULL));
	assert(remote_browser_matches_filter(TEXT("index.html"), TEXT("INDEX")));
	assert(remote_browser_matches_filter(TEXT("folder"), TEXT("old")));
	assert(!remote_browser_matches_filter(TEXT("folder"), TEXT("zip")));
	assert(!remote_browser_matches_filter(NULL, TEXT("zip")));

	printf("remote_browser_utils_exit=0\n");
	return 0;
}
