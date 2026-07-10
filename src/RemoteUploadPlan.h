#ifndef REMOTEUPLOADPLAN_H
#define REMOTEUPLOADPLAN_H

#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>

#include "FTPFile.h"

struct RemoteUploadItem {
	bool isDirectory;
	std::basic_string<TCHAR> localPath;
	std::string remotePath;
	bool remoteDirectoryExists;
	bool remoteFileExists;
	bool selected;
};

class RemoteUploadPlan {
public:
	int Build(const TCHAR * localDirectory, const char * remoteParent);
	int AddDirectory(const TCHAR * localPath, const char * remotePath);
	int AddFile(const TCHAR * localPath, const char * remotePath);
	int ApplyRemoteDirectoryListing(const char * directoryPath, const FTPFile * files, int count);
	const std::string & GetTargetPath() const;
	const std::vector<RemoteUploadItem> & GetItems() const;
	std::vector<RemoteUploadItem> & GetItems();

private:
	int AddDirectoryRecursive(const TCHAR * localDirectory, const char * remoteDirectory);

	std::vector<RemoteUploadItem> m_items;
	std::string m_targetPath;
};

struct RemoteUploadBatch {
	RemoteUploadBatch(RemoteUploadPlan * uploadPlan, const char * refreshPath);
	~RemoteUploadBatch();
	void AddRef();
	void Release();

	RemoteUploadPlan * plan;
	std::string targetPath;
	int completedCount;
	std::vector<std::basic_string<TCHAR> > failures;

private:
	volatile LONG m_references;
	RemoteUploadBatch(const RemoteUploadBatch &);
	RemoteUploadBatch & operator=(const RemoteUploadBatch &);
};

#endif //REMOTEUPLOADPLAN_H
