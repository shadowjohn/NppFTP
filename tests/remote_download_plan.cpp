#include "src/RemoteDownloadPlan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int CreateJunction(const TCHAR * link, const TCHAR * target)
{
	TCHAR fullLink[MAX_PATH]{};
	TCHAR fullTarget[MAX_PATH]{};
	TCHAR command[MAX_PATH * 3]{};
	if (GetFullPathName(link, MAX_PATH, fullLink, NULL) == 0 || GetFullPathName(target, MAX_PATH, fullTarget, NULL) == 0)
		return -1;
	if (_sntprintf_s(command, _countof(command), _TRUNCATE, TEXT("cmd.exe /D /C mklink /J \"%s\" \"%s\" >nul"), fullLink, fullTarget) < 0)
		return -1;
	return _tsystem(command);
}

int main()
{
	const TCHAR * parent = TEXT("_build\\tests\\remote_download_fixture");
	const TCHAR * root = TEXT("_build\\tests\\remote_download_fixture\\html");
	const TCHAR * assets = TEXT("_build\\tests\\remote_download_fixture\\html\\assets");
	const TCHAR * reparseParent = TEXT("_build\\tests\\remote_download_fixture_junction");
	const TCHAR * reparseTarget = TEXT("_build\\tests\\remote_download_fixture_junction_target");
	DeleteFile(TEXT("_build\\tests\\remote_download_fixture\\html\\assets\\app.js"));
	DeleteFile(TEXT("_build\\tests\\remote_download_fixture\\html\\index.html"));
	RemoveDirectory(assets);
	RemoveDirectory(root);
	RemoveDirectory(parent);
	RemoveDirectory(reparseParent);
	RemoveDirectory(reparseTarget);
	assert(CreateDirectory(TEXT("_build\\tests"), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
	assert(CreateDirectory(parent, NULL));

	const char * reservedNames[] = {
		"CON", "PRN", "AUX", "NUL", "COM1", "COM9", "LPT1", "LPT9",
		"CON.txt", "PRN.txt", "AUX.txt", "NUL.txt", "COM1.txt", "LPT1.txt"
	};
	for (size_t i = 0; i < _countof(reservedNames); ++i) {
		RemoteDownloadPlan reserved;
		std::string remotePath = std::string("/var/www/") + reservedNames[i];
		assert(reserved.Build(parent, remotePath.c_str()) != 0);
	}

	assert(CreateDirectory(reparseTarget, NULL));
	assert(CreateJunction(reparseParent, reparseTarget) == 0);
	DWORD reparseAttributes = GetFileAttributes(reparseParent);
	assert(reparseAttributes != INVALID_FILE_ATTRIBUTES && (reparseAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
	RemoteDownloadPlan reparse;
	assert(reparse.Build(reparseParent, "/var/www/html") != 0);
	assert(RemoveDirectory(reparseParent));
	assert(RemoveDirectory(reparseTarget));

	RemoteDownloadPlan plan;
	assert(plan.Build(parent, "/var/www/html") == 0);
	assert(plan.GetItems().size() == 1);
	assert(plan.GetItems()[0].isDirectory);
	assert(plan.GetItems()[0].remotePath == "/var/www/html");
	assert(plan.GetItems()[0].localPath == TEXT("_build\\tests\\remote_download_fixture\\html"));

	FTPFile files[3]{};
	lstrcpynA(files[0].filePath, "/var/www/html/index.html", MAX_PATH);
	files[0].fileType = FTPTypeFile;
	lstrcpynA(files[1].filePath, "/var/www/html/assets", MAX_PATH);
	files[1].fileType = FTPTypeDir;
	lstrcpynA(files[2].filePath, "/var/www/html/current", MAX_PATH);
	files[2].fileType = FTPTypeLink;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/html", files, 3) == 0);
	assert(plan.GetItems().size() == 3);
	assert(plan.GetItems()[1].localPath == TEXT("_build\\tests\\remote_download_fixture\\html\\index.html"));
	assert(plan.GetItems()[2].localPath == TEXT("_build\\tests\\remote_download_fixture\\html\\assets"));
	assert(plan.GetFailures().size() == 1);

	FTPFile nested{};
	lstrcpynA(nested.filePath, "/var/www/html/assets/app.js", MAX_PATH);
	nested.fileType = FTPTypeFile;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/html/assets", &nested, 1) == 0);
	assert(plan.GetItems()[3].localPath == TEXT("_build\\tests\\remote_download_fixture\\html\\assets\\app.js"));

	FTPFile escape{};
	lstrcpynA(escape.filePath, "/var/www/html/../escape.txt", MAX_PATH);
	escape.fileType = FTPTypeFile;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/html", &escape, 1) == 0);
	assert(plan.GetItems().size() == 4);
	assert(plan.GetFailures().size() == 2);

	assert(plan.PrepareLocalDirectories() == 0);
	HANDLE existing = CreateFile(plan.GetItems()[1].localPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	assert(existing != INVALID_HANDLE_VALUE);
	CloseHandle(existing);
	std::vector<size_t> collisions;
	plan.GetLocalFileCollisions(collisions);
	assert(collisions.size() == 1 && collisions[0] == 1);

	HANDLE nestedFile = CreateFile(plan.GetItems()[3].localPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	assert(nestedFile != INVALID_HANDLE_VALUE);
	CloseHandle(nestedFile);
	assert(DeleteFile(plan.GetItems()[3].localPath.c_str()));
	assert(DeleteFile(plan.GetItems()[1].localPath.c_str()));
	assert(RemoveDirectory(plan.GetItems()[2].localPath.c_str()));
	assert(RemoveDirectory(plan.GetItems()[0].localPath.c_str()));
	assert(RemoveDirectory(parent));

	printf("remote_download_plan_exit=0\n");
	return 0;
}
