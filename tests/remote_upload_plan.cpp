#include "src/RemoteUploadPlan.h"

#include <stdio.h>

static bool CreateTestFile(const TCHAR * path)
{
	HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;
	CloseHandle(file);
	return true;
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

static bool Check(bool condition, const char * message)
{
	if (condition)
		return true;
	fprintf(stderr, "remote_upload_plan_failed=%s\n", message);
	return false;
}

int main()
{
	RemoteUploadPlan plan;
	if (!Check(plan.AddDirectory(TEXT("C:\\site"), "/var/www/site") == 0, "add root directory")) return 1;
	if (!Check(plan.AddDirectory(TEXT("C:\\site\\assets"), "/var/www/site/assets") == 0, "add child directory")) return 1;
	if (!Check(plan.AddFile(TEXT("C:\\site\\index.html"), "/var/www/site/index.html") == 0, "add root file")) return 1;
	if (!Check(plan.AddFile(TEXT("C:\\site\\assets\\app.js"), "/var/www/site/assets/app.js") == 0, "add child file")) return 1;
	if (!Check(plan.GetItems().size() == 4, "plan item count")) return 1;
	if (!Check(plan.GetItems()[0].isDirectory && plan.GetItems()[1].isDirectory, "directory item types")) return 1;
	if (!Check(!plan.GetItems()[2].isDirectory && !plan.GetItems()[3].isDirectory, "file item types")) return 1;
	if (!Check(plan.GetItems()[0].remotePath == "/var/www/site", "root remote path")) return 1;
	if (!Check(plan.GetItems()[1].remotePath == "/var/www/site/assets", "child remote path")) return 1;
	if (!Check(plan.GetItems()[2].remotePath == "/var/www/site/index.html", "root file remote path")) return 1;
	if (!Check(plan.GetItems()[3].remotePath == "/var/www/site/assets/app.js", "child file remote path")) return 1;
	if (!Check(!plan.GetItems()[0].remoteDirectoryExists && !plan.GetItems()[1].remoteDirectoryExists,
		"directories initially unknown")) return 1;
	plan.GetItems()[2].selected = false;
	if (!Check(!plan.GetItems()[2].selected && plan.GetItems()[3].selected, "file selection state")) return 1;
	std::vector<const RemoteUploadItem*> directories = plan.GetDirectoryItems();
	if (!Check(directories.size() == 2, "directory preparation count")) return 1;
	if (!Check(directories[0]->remotePath == "/var/www/site", "parent directory prepares first")) return 1;
	if (!Check(directories[1]->remotePath == "/var/www/site/assets", "child directory prepares second")) return 1;
	std::vector<const RemoteUploadItem*> selectedFiles = plan.GetSelectedFileItems();
	if (!Check(selectedFiles.size() == 1, "only selected files dispatch")) return 1;
	if (!Check(selectedFiles[0]->remotePath == "/var/www/site/assets/app.js", "selected file identity")) return 1;
	if (!Check(plan.GetSkippedFileCount() == 1, "skipped file count")) return 1;

	RemoteUploadPlan unordered;
	if (!Check(unordered.AddDirectory(TEXT("C:\\site\\assets\\js"), "/var/www/site/assets/js") == 0, "add unordered grandchild")) return 1;
	if (!Check(unordered.AddDirectory(TEXT("C:\\site"), "/var/www/site") == 0, "add unordered parent")) return 1;
	if (!Check(unordered.AddDirectory(TEXT("C:\\site\\assets"), "/var/www/site/assets") == 0, "add unordered child")) return 1;
	std::vector<const RemoteUploadItem*> orderedDirectories = unordered.GetDirectoryItems();
	if (!Check(orderedDirectories.size() == 3, "unordered directory preparation count")) return 1;
	if (!Check(orderedDirectories[0]->remotePath == "/var/www/site", "helper sorts parent first")) return 1;
	if (!Check(orderedDirectories[1]->remotePath == "/var/www/site/assets", "helper sorts child second")) return 1;
	if (!Check(orderedDirectories[2]->remotePath == "/var/www/site/assets/js", "helper sorts grandchild last")) return 1;

	FTPFile targetListing{};
	lstrcpynA(targetListing.filePath, "/var/www/site", MAX_PATH);
	targetListing.fileType = FTPTypeLink;
	if (!Check(plan.ApplyRemoteDirectoryListing("/var/www", &targetListing, 1) == 0, "apply target listing")) return 1;
	if (!Check(plan.GetItems()[0].remoteDirectoryExists, "link target counts as existing directory")) return 1;

	FTPFile listed[2]{};
	lstrcpynA(listed[0].filePath, "/var/www/site/index.html", MAX_PATH);
	listed[0].fileType = FTPTypeFile;
	lstrcpynA(listed[1].filePath, "/var/www/site/assets", MAX_PATH);
	listed[1].fileType = FTPTypeDir;
	if (!Check(plan.ApplyRemoteDirectoryListing("/var/www/site", listed, 2) == 0, "apply site listing")) return 1;
	if (!Check(plan.GetItems()[1].remoteDirectoryExists, "listed child directory exists")) return 1;
	if (!Check(plan.GetItems()[2].remoteFileExists, "listed root file exists")) return 1;
	if (!Check(!plan.GetItems()[3].remoteFileExists, "unlisted child file remains absent")) return 1;

	FTPFile listedDirectory{};
	lstrcpynA(listedDirectory.filePath, "/var/www/site/assets/app.js", MAX_PATH);
	listedDirectory.fileType = FTPTypeDir;
	if (!Check(plan.ApplyRemoteDirectoryListing("/var/www/site/assets", &listedDirectory, 1) == 0, "apply child directory listing")) return 1;
	if (!Check(!plan.GetItems()[3].remoteFileExists, "directory does not count as file")) return 1;
	listedDirectory.fileType = FTPTypeLink;
	if (!Check(plan.ApplyRemoteDirectoryListing("/var/www/site/assets", &listedDirectory, 1) == 0, "apply child link listing")) return 1;
	if (!Check(plan.GetItems()[3].remoteFileExists, "link counts as existing file")) return 1;

	const TCHAR * fixture = TEXT("_build\\tests\\remote_upload_fixture");
	const TCHAR * assets = TEXT("_build\\tests\\remote_upload_fixture\\assets");
	const TCHAR * indexFile = TEXT("_build\\tests\\remote_upload_fixture\\index.html");
	const TCHAR * appFile = TEXT("_build\\tests\\remote_upload_fixture\\assets\\app.js");
	DeleteFile(appFile);
	DeleteFile(indexFile);
	RemoveDirectory(assets);
	RemoveDirectory(fixture);
	if (!Check(CreateDirectory(fixture, NULL) != FALSE, "create fixture directory")) return 1;
	if (!Check(CreateDirectory(assets, NULL) != FALSE, "create fixture child directory")) return 1;
	if (!Check(CreateTestFile(indexFile), "create fixture root file")) return 1;
	if (!Check(CreateTestFile(appFile), "create fixture child file")) return 1;

	RemoteUploadPlan built;
	if (!Check(built.Build(fixture, "/var/www") == 0, "build fixture plan")) return 1;
	if (!Check(built.GetTargetPath() == "/var/www", "built target path")) return 1;
	if (!Check(built.GetItems().size() == 4, "built item count")) return 1;
	if (!Check(HasRemotePath(built, "/var/www/remote_upload_fixture"), "built root path")) return 1;
	if (!Check(HasRemotePath(built, "/var/www/remote_upload_fixture/assets"), "built child path")) return 1;
	if (!Check(HasRemotePath(built, "/var/www/remote_upload_fixture/index.html"), "built root file path")) return 1;
	if (!Check(HasRemotePath(built, "/var/www/remote_upload_fixture/assets/app.js"), "built child file path")) return 1;

	if (!Check(DeleteFile(appFile) != FALSE, "delete fixture child file")) return 1;
	if (!Check(DeleteFile(indexFile) != FALSE, "delete fixture root file")) return 1;
	if (!Check(RemoveDirectory(assets) != FALSE, "remove fixture child directory")) return 1;
	if (!Check(RemoveDirectory(fixture) != FALSE, "remove fixture directory")) return 1;

	RemoteUploadPlan * ownedPlan = new RemoteUploadPlan;
	RemoteUploadBatch * batch = new RemoteUploadBatch(ownedPlan, "/var/www");
	if (!Check(batch->plan == ownedPlan, "batch owns plan")) return 1;
	if (!Check(batch->targetPath == "/var/www", "batch target path")) return 1;
	if (!Check(batch->completedCount == 0, "batch initial completion count")) return 1;
	batch->InitializeFileCounts(2, 1);
	if (!Check(batch->GetSelectedFileCount() == 2, "batch selected count")) return 1;
	if (!Check(batch->GetSkippedFileCount() == 1, "batch skipped count")) return 1;
	if (!Check(batch->GetRemainingFileCount() == 2, "batch initial remaining count")) return 1;
	if (!Check(!batch->CompleteFileTerminal(), "first terminal does not request completion")) return 1;
	if (!Check(batch->CompleteFileTerminal(), "last terminal requests completion")) return 1;
	if (!Check(!batch->RequestCompletionIfReady(), "completion marker is requested exactly once")) return 1;
	batch->AddRef();
	batch->Release();
	batch->Release();

	RemoteUploadBatch * failedCanceledBatch = new RemoteUploadBatch(new RemoteUploadPlan, "/var/www");
	failedCanceledBatch->InitializeFileCounts(2, 0);
	failedCanceledBatch->RecordFileFailed();
	if (!Check(!failedCanceledBatch->CompleteFileTerminal(), "failed file remains terminal work")) return 1;
	failedCanceledBatch->RecordCanceled("/var/www/canceled.txt");
	if (!Check(failedCanceledBatch->CompleteFileTerminal(), "canceled last file requests completion")) return 1;
	if (!Check(failedCanceledBatch->GetFailedFileCount() == 1, "failed file count")) return 1;
	if (!Check(failedCanceledBatch->GetCanceledFileCount() == 1, "canceled file count")) return 1;
	std::vector<std::string> canceledPaths;
	failedCanceledBatch->GetCanceledPaths(canceledPaths);
	if (!Check(canceledPaths.size() == 1 && canceledPaths[0] == "/var/www/canceled.txt", "canceled path")) return 1;
	failedCanceledBatch->Release();

	RemoteUploadPlan * fatalPlan = new RemoteUploadPlan;
	if (!Check(fatalPlan->AddDirectory(TEXT("C:\\site"), "/var/www/site") == 0, "fatal plan directory")) return 1;
	if (!Check(fatalPlan->AddFile(TEXT("C:\\site\\one.txt"), "/var/www/site/one.txt") == 0, "fatal plan first file")) return 1;
	if (!Check(fatalPlan->AddFile(TEXT("C:\\site\\two.txt"), "/var/www/site/two.txt") == 0, "fatal plan second file")) return 1;
	RemoteUploadBatch * fatalBatch = new RemoteUploadBatch(fatalPlan, "/var/www");
	fatalBatch->InitializeFileCounts(2, 0);
	if (!Check(fatalBatch->CancelUnstartedSelectedFiles(), "fatal preparation requests one completion")) return 1;
	if (!Check(fatalBatch->GetCanceledFileCount() == 2, "fatal preparation cancels selected files")) return 1;
	if (!Check(fatalBatch->GetRemainingFileCount() == 0, "fatal preparation reaches terminal zero")) return 1;
	if (!Check(!fatalBatch->CancelUnstartedSelectedFiles(), "fatal preparation completion is one-shot")) return 1;
	fatalBatch->Release();
	printf("remote_upload_plan_exit=0\n");
	return 0;
}
