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
	assert(plan.GetItems()[0].remotePath == "/var/www/site");
	assert(plan.GetItems()[1].remotePath == "/var/www/site/assets");
	assert(plan.GetItems()[2].remotePath == "/var/www/site/index.html");
	assert(plan.GetItems()[3].remotePath == "/var/www/site/assets/app.js");
	assert(!plan.GetItems()[0].remoteDirectoryExists);
	assert(!plan.GetItems()[1].remoteDirectoryExists);
	plan.GetItems()[2].selected = false;
	assert(!plan.GetItems()[2].selected);
	assert(plan.GetItems()[3].selected);

	FTPFile targetListing{};
	lstrcpynA(targetListing.filePath, "/var/www/site", MAX_PATH);
	targetListing.fileType = FTPTypeLink;
	assert(plan.ApplyRemoteDirectoryListing("/var/www", &targetListing, 1) == 0);
	assert(plan.GetItems()[0].remoteDirectoryExists);

	FTPFile listed[2]{};
	lstrcpynA(listed[0].filePath, "/var/www/site/index.html", MAX_PATH);
	listed[0].fileType = FTPTypeFile;
	lstrcpynA(listed[1].filePath, "/var/www/site/assets", MAX_PATH);
	listed[1].fileType = FTPTypeDir;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/site", listed, 2) == 0);
	assert(plan.GetItems()[1].remoteDirectoryExists);
	assert(plan.GetItems()[2].remoteFileExists);
	assert(!plan.GetItems()[3].remoteFileExists);

	FTPFile listedDirectory{};
	lstrcpynA(listedDirectory.filePath, "/var/www/site/assets/app.js", MAX_PATH);
	listedDirectory.fileType = FTPTypeDir;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/site/assets", &listedDirectory, 1) == 0);
	assert(!plan.GetItems()[3].remoteFileExists);
	listedDirectory.fileType = FTPTypeLink;
	assert(plan.ApplyRemoteDirectoryListing("/var/www/site/assets", &listedDirectory, 1) == 0);
	assert(plan.GetItems()[3].remoteFileExists);

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
	assert(built.GetTargetPath() == "/var/www");
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

	RemoteUploadPlan * ownedPlan = new RemoteUploadPlan;
	RemoteUploadBatch * batch = new RemoteUploadBatch(ownedPlan, "/var/www");
	assert(batch->plan == ownedPlan);
	assert(batch->targetPath == "/var/www");
	assert(batch->completedCount == 0);
	batch->AddRef();
	batch->Release();
	batch->Release();
	printf("remote_upload_plan_exit=0\n");
	return 0;
}
