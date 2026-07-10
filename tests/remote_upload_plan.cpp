#include "src/RemoteUploadPlan.h"

#include <assert.h>
#include <stdio.h>

static void CreateTestFile(const TCHAR * path)
{
	HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	assert(file != INVALID_HANDLE_VALUE);
	CloseHandle(file);
}

static bool HasRemotePath(const RemoteUploadPlan & plan, const char * path)
{
	const std::vector<RemoteUploadItem> & items = plan.GetItems();
	for (size_t i = 0; i < items.size(); ++i) {
		if (items[i].remotePath == path)
			return true;
	}
	return false;
}

int main()
{
	RemoteUploadPlan plan;
	assert(plan.AddDirectory(TEXT("C:\\site"), "/var/www/site") == 0);
	assert(plan.AddDirectory(TEXT("C:\\site\\assets"), "/var/www/site/assets") == 0);
	assert(plan.AddFile(TEXT("C:\\site\\index.html"), "/var/www/site/index.html") == 0);
	assert(plan.AddFile(TEXT("C:\\site\\assets\\app.js"), "/var/www/site/assets/app.js") == 0);
	assert(plan.GetItems().size() == 4);
	assert(plan.GetItems()[0].isDirectory);
	assert(plan.GetItems()[1].isDirectory);
	assert(!plan.GetItems()[2].isDirectory);
	assert(!plan.GetItems()[3].isDirectory);

	FTPFile listed{};
	lstrcpynA(listed.filePath, "/var/www/site/index.html", MAX_PATH);
	listed.fileType = FTPTypeFile;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/site", &listed, 1) == 0);
	assert(plan.GetItems()[2].remoteFileExists);
	assert(!plan.GetItems()[3].remoteFileExists);

	FTPFile listedDirectory{};
	lstrcpynA(listedDirectory.filePath, "/var/www/site/assets/app.js", MAX_PATH);
	listedDirectory.fileType = FTPTypeDir;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/site/assets", &listedDirectory, 1) == 0);
	assert(!plan.GetItems()[3].remoteFileExists);

	const TCHAR * fixture = TEXT("_build\\tests\\remote_upload_fixture");
	const TCHAR * assets = TEXT("_build\\tests\\remote_upload_fixture\\assets");
	const TCHAR * indexFile = TEXT("_build\\tests\\remote_upload_fixture\\index.html");
	const TCHAR * appFile = TEXT("_build\\tests\\remote_upload_fixture\\assets\\app.js");
	DeleteFile(appFile);
	DeleteFile(indexFile);
	RemoveDirectory(assets);
	RemoveDirectory(fixture);
	assert(CreateDirectory(fixture, NULL));
	assert(CreateDirectory(assets, NULL));
	CreateTestFile(indexFile);
	CreateTestFile(appFile);

	RemoteUploadPlan built;
	assert(built.Build(fixture, "/var/www") == 0);
	assert(built.GetItems().size() == 4);
	assert(built.GetItems()[0].isDirectory);
	assert(built.GetItems()[1].isDirectory);
	assert(!built.GetItems()[2].isDirectory);
	assert(!built.GetItems()[3].isDirectory);
	assert(HasRemotePath(built, "/var/www/remote_upload_fixture"));
	assert(HasRemotePath(built, "/var/www/remote_upload_fixture/assets"));
	assert(HasRemotePath(built, "/var/www/remote_upload_fixture/index.html"));
	assert(HasRemotePath(built, "/var/www/remote_upload_fixture/assets/app.js"));

	assert(DeleteFile(appFile));
	assert(DeleteFile(indexFile));
	assert(RemoveDirectory(assets));
	assert(RemoveDirectory(fixture));
	printf("remote_upload_plan_exit=0\n");
	return 0;
}
