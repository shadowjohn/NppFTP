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
	const TCHAR * ancestorJunction = TEXT("_build\\tests\\remote_download_fixture_ancestor_junction");
	const TCHAR * ancestorTarget = TEXT("_build\\tests\\remote_download_fixture_ancestor_target");
	const TCHAR * ancestorSelected = TEXT("_build\\tests\\remote_download_fixture_ancestor_junction\\selected");
	const TCHAR * nestedReparseTarget = TEXT("_build\\tests\\remote_download_fixture_nested_target");
	const TCHAR * localDirectoryCollisionRoot = TEXT("_build\\tests\\remote_download_fixture\\collision-root");
	const TCHAR * localDirectoryCollisionPath = TEXT("_build\\tests\\remote_download_fixture\\collision-root\\report.txt");
	DeleteFile(TEXT("_build\\tests\\remote_download_fixture\\html\\assets\\app.js"));
	DeleteFile(TEXT("_build\\tests\\remote_download_fixture\\html\\index.html"));
	RemoveDirectory(localDirectoryCollisionPath);
	RemoveDirectory(localDirectoryCollisionRoot);
	RemoveDirectory(assets);
	RemoveDirectory(root);
	RemoveDirectory(parent);
	RemoveDirectory(reparseParent);
	RemoveDirectory(reparseTarget);
	RemoveDirectory(ancestorSelected);
	RemoveDirectory(ancestorJunction);
	RemoveDirectory(ancestorTarget);
	RemoveDirectory(nestedReparseTarget);
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

	assert(CreateDirectory(ancestorTarget, NULL));
	assert(CreateJunction(ancestorJunction, ancestorTarget) == 0);
	assert(CreateDirectory(ancestorSelected, NULL));
	DWORD ancestorAttributes = GetFileAttributes(ancestorSelected);
	assert(ancestorAttributes != INVALID_FILE_ATTRIBUTES && (ancestorAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
	RemoteDownloadPlan ancestor;
	assert(ancestor.Build(ancestorSelected, "/var/www/html") != 0);
	assert(RemoveDirectory(ancestorSelected));
	assert(RemoveDirectory(ancestorJunction));
	assert(RemoveDirectory(ancestorTarget));

	RemoteDownloadPlan nestedReparse;
	assert(nestedReparse.Build(parent, "/var/www/html") == 0);
	assert(CreateDirectory(root, NULL));
	assert(CreateDirectory(nestedReparseTarget, NULL));
	assert(CreateJunction(assets, nestedReparseTarget) == 0);
	FTPFile nestedReparseFile{};
	lstrcpynA(nestedReparseFile.filePath, "/var/www/html/assets/app.js", MAX_PATH);
	nestedReparseFile.fileType = FTPTypeFile;
	assert(nestedReparse.ApplyRemoteDirectoryListing("/var/www/html/assets", &nestedReparseFile, 1) == 0);
	assert(nestedReparse.GetItems().size() == 1);
	assert(nestedReparse.GetFailures().size() == 1);
	assert(RemoveDirectory(assets));
	assert(RemoveDirectory(root));
	assert(RemoveDirectory(nestedReparseTarget));

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
	assert(plan.GetItems()[1].selected);
	plan.GetItems()[1].selected = false;
	assert(!plan.GetItems()[1].selected);
	plan.GetItems()[1].selected = true;

	RemoteDownloadPlan localDirectoryCollision;
	assert(localDirectoryCollision.Build(parent, "/var/www/collision-root") == 0);
	FTPFile localDirectoryCollisionFile{};
	lstrcpynA(localDirectoryCollisionFile.filePath, "/var/www/collision-root/report.txt", MAX_PATH);
	localDirectoryCollisionFile.fileType = FTPTypeFile;
	assert(localDirectoryCollision.ApplyRemoteDirectoryListing("/var/www/collision-root", &localDirectoryCollisionFile, 1) == 0);
	assert(CreateDirectory(localDirectoryCollision.GetItems()[0].localPath.c_str(), NULL));
	assert(CreateDirectory(localDirectoryCollision.GetItems()[1].localPath.c_str(), NULL));
	assert(localDirectoryCollision.PrepareLocalDirectories() == 0);
	assert(!localDirectoryCollision.GetItems()[1].selected);
	assert(localDirectoryCollision.GetFailures().size() == 1);
	assert(RemoveDirectory(localDirectoryCollision.GetItems()[1].localPath.c_str()));
	assert(RemoveDirectory(localDirectoryCollision.GetItems()[0].localPath.c_str()));

	RemoteDownloadPlan caseCollision;
	assert(caseCollision.Build(parent, "/var/www/case-root") == 0);
	FTPFile caseFiles[2]{};
	lstrcpynA(caseFiles[0].filePath, "/var/www/case-root/Readme.txt", MAX_PATH);
	caseFiles[0].fileType = FTPTypeFile;
	lstrcpynA(caseFiles[1].filePath, "/var/www/case-root/readme.txt", MAX_PATH);
	caseFiles[1].fileType = FTPTypeFile;
	assert(caseCollision.ApplyRemoteDirectoryListing("/var/www/case-root", caseFiles, 2) == 0);
	assert(caseCollision.GetItems().size() == 3);
	assert(caseCollision.GetItems()[1].selected);
	assert(!caseCollision.GetItems()[2].selected);
	assert(caseCollision.GetFailures().size() == 1);

	RemoteDownloadPlan unicodeCaseCollision;
	assert(unicodeCaseCollision.Build(parent, "/var/www/unicode-case-root") == 0);
	FTPFile unicodeCaseFiles[2]{};
	lstrcpynA(unicodeCaseFiles[0].filePath, "/var/www/unicode-case-root/\xC3\x84.txt", MAX_PATH);
	unicodeCaseFiles[0].fileType = FTPTypeFile;
	lstrcpynA(unicodeCaseFiles[1].filePath, "/var/www/unicode-case-root/\xC3\xA4.txt", MAX_PATH);
	unicodeCaseFiles[1].fileType = FTPTypeFile;
	assert(unicodeCaseCollision.ApplyRemoteDirectoryListing("/var/www/unicode-case-root", unicodeCaseFiles, 2) == 0);

#ifdef UNICODE
	assert(unicodeCaseCollision.GetItems().size() == 3);
	assert(unicodeCaseCollision.GetItems()[1].selected);
	assert(!unicodeCaseCollision.GetItems()[2].selected);
	assert(unicodeCaseCollision.GetFailures().size() == 1);
#else
	if (unicodeCaseCollision.GetItems().size() == 3) {
		assert(unicodeCaseCollision.GetItems()[1].selected);
		assert(!unicodeCaseCollision.GetItems()[2].selected);
		assert(unicodeCaseCollision.GetFailures().size() == 1);
	} else {
		assert(unicodeCaseCollision.GetItems().size() >= 1 && unicodeCaseCollision.GetItems().size() < 3);
		assert(unicodeCaseCollision.GetFailures().size() == 3 - unicodeCaseCollision.GetItems().size());
	}
#endif

	RemoteDownloadPlan * owned = new RemoteDownloadPlan;
	RemoteDownloadBatch * batch = new RemoteDownloadBatch(owned);
	assert(batch->plan == owned);
	assert(batch->completedCount == 0);
	batch->AddRef();
	batch->Release();
	batch->Release();

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
