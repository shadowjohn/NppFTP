#include "src/StdInc.h"
#include "src/FTPClientWrapper.h"
#include "src/QueueOperation.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

Output * _MainOutput = NULL;
HWND _MainOutputWindow = NULL;

int OutputDebug(const TCHAR *, ...) { return 0; }
int OutputMsg(const TCHAR *, ...) { return 0; }
int OutputClnt(const TCHAR *, ...) { return 0; }
int OutputErr(const TCHAR *, ...) { return 0; }
int MessageBoxOutput(const TCHAR *) { return 0; }

static bool Check(bool condition, const char * message)
{
	if (condition)
		return true;
	fprintf(stderr, "remote_recursive_delete_failed=%s\n", message);
	return false;
}

class RecursiveDeleteWrapper : public FTPClientWrapper {
public:
	RecursiveDeleteWrapper(const char * failDeletePath = NULL, bool injectOutsideEntry = false) :
		FTPClientWrapper(Client_SSH, "test", 22, "user", "password"),
		m_failDeletePath(failDeletePath ? failDeletePath : ""),
		m_injectOutsideEntry(injectOutsideEntry),
		m_failure(RemoteFailureUnknown)
	{
	}

	virtual FTPClientWrapper * Clone() { return NULL; }
	virtual RemoteFailureKind GetFailureKind() const { return m_failure; }
	virtual int Connect() { m_connected = true; return 0; }
	virtual int Disconnect() { m_connected = false; return 0; }
	virtual int NoOp() { return 0; }
	virtual int GetDir(const char * path, FTPFile ** files)
	{
		if (!files)
			return -1;
		*files = NULL;
		std::vector<FTPFile> entries;
		if (strcmp(path, "/var/www/html/test/test_upload") == 0) {
			if (m_injectOutsideEntry) {
				entries.push_back(MakeEntry("/var/www/html/test/other.txt", FTPTypeFile));
			} else {
				entries.push_back(MakeEntry("/var/www/html/test/test_upload/index.php", FTPTypeFile));
				entries.push_back(MakeEntry("/var/www/html/test/test_upload/current", FTPTypeLink));
				entries.push_back(MakeEntry("/var/www/html/test/test_upload/assets", FTPTypeDir));
			}
		} else if (strcmp(path, "/var/www/html/test/test_upload/assets") == 0) {
			entries.push_back(MakeEntry("/var/www/html/test/test_upload/assets/app.js", FTPTypeFile));
		} else if (strcmp(path, "/blocked") == 0) {
			entries.push_back(MakeEntry("/blocked/locked.txt", FTPTypeFile));
		} else {
			m_failure = RemoteFailureNotFound;
			return -1;
		}

		*files = new FTPFile[entries.size()];
		memcpy(*files, &entries[0], sizeof(FTPFile) * entries.size());
		return static_cast<int>(entries.size());
	}
	virtual int Cwd(const char *) { return 0; }
	virtual int Pwd(char *, size_t) { return 0; }
	virtual int Rename(const char *, const char *) { return 0; }
	virtual int ChmodFile(const char *, const char *) { return 0; }
	virtual int MkDir(const char *) { return 0; }
	virtual int RmDir(const char * path)
	{
		m_operations.push_back(std::string("dir:") + path);
		if (m_failDeletePath == path) {
			m_failure = RemoteFailurePermissionDenied;
			return -1;
		}
		return 0;
	}
	virtual int MkFile(const char *) { return 0; }
	virtual int SendFile(const TCHAR *, const char *) { return 0; }
	virtual int ReceiveFile(const TCHAR *, const char *) { return 0; }
	virtual int SendFile(HANDLE, const char *) { return 0; }
	virtual int ReceiveFile(HANDLE, const char *) { return 0; }
	virtual int DeleteFile(const char * path)
	{
		m_operations.push_back(std::string("file:") + path);
		if (m_failDeletePath == path) {
			m_failure = RemoteFailurePermissionDenied;
			return -1;
		}
		return 0;
	}
	virtual DWORD LastAction() { return 0; }

	const std::vector<std::string> & Operations() const { return m_operations; }

private:
	static FTPFile MakeEntry(const char * path, FTPFileType type)
	{
		FTPFile entry{};
		lstrcpynA(entry.filePath, path, MAX_PATH + 1);
		entry.fileType = type;
		return entry;
	}

	std::string m_failDeletePath;
	bool m_injectOutsideEntry;
	RemoteFailureKind m_failure;
	std::vector<std::string> m_operations;
};

class TestQueueRemoveDirRecursive : public QueueRemoveDirRecursive {
public:
	TestQueueRemoveDirRecursive(const char * path) : QueueRemoveDirRecursive(NULL, path) {}
	using QueueOperation::SetClient;
};

int main()
{
	RecursiveDeleteWrapper wrapper;
	TestQueueRemoveDirRecursive operation("/var/www/html/test/test_upload");
	operation.SetClient(&wrapper);
	if (!Check(operation.Perform() == 0, "recursive delete succeeds")) return 1;
	if (!Check(operation.GetDeletedFileCount() == 3, "files and symlinks are deleted")) return 1;
	if (!Check(operation.GetDeletedDirectoryCount() == 2, "nested and root directories are deleted")) return 1;
	const std::vector<std::string> & operations = wrapper.Operations();
	if (!Check(operations.size() == 5, "recursive delete emits five remote mutations")) return 1;
	if (!Check(operations[0] == "file:/var/www/html/test/test_upload/index.php", "root file is removed first")) return 1;
	if (!Check(operations[1] == "file:/var/www/html/test/test_upload/current", "symlink itself is removed without traversal")) return 1;
	if (!Check(operations[2] == "file:/var/www/html/test/test_upload/assets/app.js", "nested file is removed before its directory")) return 1;
	if (!Check(operations[3] == "dir:/var/www/html/test/test_upload/assets", "nested directory is removed before parent")) return 1;
	if (!Check(operations[4] == "dir:/var/www/html/test/test_upload", "requested directory is removed last")) return 1;

	RecursiveDeleteWrapper rootWrapper;
	TestQueueRemoveDirRecursive rootOperation("/");
	rootOperation.SetClient(&rootWrapper);
	if (!Check(rootOperation.Perform() == -1, "root directory deletion is rejected")) return 1;
	if (!Check(rootWrapper.Operations().empty(), "root rejection performs no remote mutation")) return 1;

	RecursiveDeleteWrapper outsideWrapper(NULL, true);
	TestQueueRemoveDirRecursive outsideOperation("/var/www/html/test/test_upload");
	outsideOperation.SetClient(&outsideWrapper);
	if (!Check(outsideOperation.Perform() == -1, "outside listing entry is rejected")) return 1;
	if (!Check(outsideWrapper.Operations().empty(), "outside listing entry performs no remote mutation")) return 1;
	if (!Check(strcmp(outsideOperation.GetFailedPath(), "/var/www/html/test/test_upload") == 0, "outside listing entry identifies its parent")) return 1;

	RecursiveDeleteWrapper failedWrapper("/var/www/html/test/test_upload/assets/app.js");
	TestQueueRemoveDirRecursive failedOperation("/var/www/html/test/test_upload");
	failedOperation.SetClient(&failedWrapper);
	if (!Check(failedOperation.Perform() == -1, "delete failure stops recursive operation")) return 1;
	if (!Check(strcmp(failedOperation.GetFailedPath(), "/var/www/html/test/test_upload/assets/app.js") == 0, "delete failure identifies the exact path")) return 1;
	if (!Check(failedOperation.GetFailureKind() == RemoteFailurePermissionDenied, "delete failure retains remote permission detail")) return 1;
	if (!Check(failedWrapper.Operations().size() == 3, "nested file failure never removes ancestor directories")) return 1;

	RecursiveDeleteWrapper firstFileWrapper("/var/www/html/test/test_upload/index.php");
	TestQueueRemoveDirRecursive firstFileOperation("/var/www/html/test/test_upload");
	firstFileOperation.SetClient(&firstFileWrapper);
	if (!Check(firstFileOperation.Perform() == -1, "first file failure stops deletion")) return 1;
	if (!Check(strcmp(firstFileOperation.GetFailedPath(), "/var/www/html/test/test_upload/index.php") == 0, "first file failure survives listing release")) return 1;
	if (!Check(firstFileOperation.GetFailureKind() == RemoteFailurePermissionDenied, "first file failure retains permission detail")) return 1;
	if (!Check(firstFileWrapper.Operations().size() == 1, "first file failure prevents later mutations")) return 1;

	RecursiveDeleteWrapper directoryFailureWrapper("/var/www/html/test/test_upload/assets");
	TestQueueRemoveDirRecursive directoryFailureOperation("/var/www/html/test/test_upload");
	directoryFailureOperation.SetClient(&directoryFailureWrapper);
	if (!Check(directoryFailureOperation.Perform() == -1, "child directory failure stops deletion")) return 1;
	if (!Check(strcmp(directoryFailureOperation.GetFailedPath(), "/var/www/html/test/test_upload/assets") == 0, "parent listing release preserves the child directory failure path")) return 1;
	if (!Check(directoryFailureOperation.GetFailureKind() == RemoteFailurePermissionDenied, "child directory failure retains permission detail")) return 1;
	if (!Check(directoryFailureWrapper.Operations().size() == 4, "child directory failure never removes the parent")) return 1;

	printf("remote_recursive_delete_exit=0\n");
	return 0;
}
