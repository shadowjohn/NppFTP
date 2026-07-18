#include "src/remote_browser_utils.h"
#include "src/RemoteFailure.h"

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
	remote_browser_format_size(false, 512LL, text, 64);
	assert(_tcscmp(text, TEXT("512.00 B")) == 0);
	remote_browser_format_size(false, 1536LL, text, 64);
	assert(_tcscmp(text, TEXT("1.50 KB")) == 0);
	remote_browser_format_size(false, 18864384LL, text, 64);
	assert(_tcscmp(text, TEXT("17.99 MB")) == 0);
	remote_browser_format_size(false, 5368709120LL, text, 64);
	assert(_tcscmp(text, TEXT("5.00 GB")) == 0);
	remote_browser_format_size(false, 3298534883328LL, text, 64);
	assert(_tcscmp(text, TEXT("3.00 TB")) == 0);
	remote_browser_format_size(true, 5368709120LL, text, 64);
	assert(_tcscmp(text, TEXT("")) == 0);
	remote_browser_format_size(false, -1LL, text, 64);
	assert(_tcscmp(text, TEXT("")) == 0);

	SYSTEMTIME modified{};
	modified.wYear = 2026;
	modified.wMonth = 7;
	modified.wDay = 11;
	modified.wHour = 21;
	modified.wMinute = 1;
	TCHAR modifiedText[32]{};
	remote_browser_format_system_time(modified, modifiedText, 32);
	assert(_tcscmp(modifiedText, TEXT("2026-07-11 21:01:00")) == 0);

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

	TCHAR mode[4]{};
	bool checks[9]{};
	assert(remote_browser_permission_to_octal("drwxr-xr-x", mode, 4) == 0);
	assert(_tcscmp(mode, TEXT("755")) == 0);
	assert(remote_browser_permission_to_octal("-rw-r-----", mode, 4) == 0);
	assert(_tcscmp(mode, TEXT("640")) == 0);
	assert(remote_browser_permission_to_octal(NULL, mode, 4) == -1);
	assert(remote_browser_octal_to_checks(TEXT("755"), checks) == 0);
	assert(checks[0] && checks[1] && checks[2]);
	assert(checks[3] && !checks[4] && checks[5]);
	assert(checks[6] && !checks[7] && checks[8]);
	assert(remote_browser_checks_to_octal(checks, mode, 4) == 0);
	assert(_tcscmp(mode, TEXT("755")) == 0);
	assert(remote_browser_octal_to_checks(TEXT("89x"), checks) == -1);
	assert(RemoteOverwriteCancel == 0);
	assert(RemoteOverwriteOverwrite == 1);
	assert(RemoteOverwriteSkip == 2);
	assert(RemoteFailureUnknown == 0);
	assert(RemoteFailurePermissionDenied != RemoteFailureNotFound);
	assert(remote_browser_directory_result_succeeded(0));
	assert(remote_browser_directory_result_succeeded(12));
	assert(!remote_browser_directory_result_succeeded(-1));
	assert(remote_browser_pending_path_matches("/target", "/target"));
	assert(!remote_browser_pending_path_matches("/newest", "/older"));
	assert(!remote_browser_pending_path_matches("", "/target"));
	assert(!remote_browser_pending_path_matches(NULL, "/target"));
	const char * staleATarget = "/foo";
	const char * pendingB = "/";
	const char * staleAParent = "/";
	assert(!remote_browser_pending_path_matches(pendingB, staleATarget));
	assert(!remote_browser_refresh_commits_pending(false, pendingB, staleAParent));
	assert(remote_browser_refresh_commits_pending(true, pendingB, staleAParent));

	printf("remote_browser_utils_exit=0\n");
	return 0;
}
