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

enum RemoteUploadFileOutcome {
	RemoteUploadFileSucceeded,
	RemoteUploadFileFailed,
	RemoteUploadFileCanceled
};

RemoteUploadFileOutcome resolve_remote_upload_file_outcome(int result, bool canceled);
bool should_dispatch_remote_upload_after_prepare(int result, bool canceled);

const unsigned int NotifyMessageRemoteUploadBatchComplete = WM_USER + 506;

class RemoteUploadPlan {
public:
	int Build(const TCHAR * localDirectory, const char * remoteParent);
	int AddDirectory(const TCHAR * localPath, const char * remotePath);
	int AddFile(const TCHAR * localPath, const char * remotePath);
	int ApplyRemoteDirectoryListing(const char * directoryPath, const FTPFile * files, int count);
	const std::string & GetTargetPath() const;
	const std::vector<RemoteUploadItem> & GetItems() const;
	std::vector<RemoteUploadItem> & GetItems();
	std::vector<const RemoteUploadItem*> GetDirectoryItems() const;
	std::vector<const RemoteUploadItem*> GetSelectedFileItems() const;
	int GetSkippedFileCount() const;

private:
	int AddDirectoryRecursive(const TCHAR * localDirectory, const char * remoteDirectory);

	std::vector<RemoteUploadItem> m_items;
	std::string m_targetPath;
};

struct RemoteUploadBatch {
	RemoteUploadBatch(RemoteUploadPlan * uploadPlan, const char * refreshPath,
		HWND completionWindow = NULL, LONG completionGeneration = 0);
	~RemoteUploadBatch();
	void AddRef();
	void Release();
	void InitializeFileCounts(int selectedFiles, int skippedFiles);
	void RecordFileSucceeded();
	void RecordFileFailed();
	void RecordCanceled(const char * remotePath);
	bool CancelUnstartedSelectedFiles();
	bool CompleteFileTerminal();
	bool RequestCompletionIfReady();
	int GetSelectedFileCount() const;
	int GetSkippedFileCount() const;
	int GetFailedFileCount() const;
	int GetCanceledFileCount() const;
	int GetRemainingFileCount() const;
	void GetCanceledPaths(std::vector<std::string> & paths) const;

	RemoteUploadPlan * plan;
	std::string targetPath;
	volatile LONG completedCount;
	std::vector<std::basic_string<TCHAR> > failures;

private:
	volatile LONG m_references;
	volatile LONG m_selectedFiles;
	volatile LONG m_skippedFiles;
	volatile LONG m_failedFiles;
	volatile LONG m_canceledFiles;
	volatile LONG m_remainingFiles;
	volatile LONG m_completionRequested;
	volatile LONG m_unstartedCancellationRecorded;
	HWND m_completionWindow;
	LONG m_completionGeneration;
	mutable CRITICAL_SECTION m_canceledPathsLock;
	std::vector<std::string> m_canceledPaths;
	RemoteUploadBatch(const RemoteUploadBatch &);
	RemoteUploadBatch & operator=(const RemoteUploadBatch &);
};

class RemoteUploadFileTerminalState {
public:
	RemoteUploadFileTerminalState(RemoteUploadBatch * batch, const char * remotePath);
	void Cancel();
	bool Complete();
	bool WasCanceled() const;

private:
	RemoteUploadBatch * m_batch;
	std::string m_remotePath;
	volatile LONG m_canceled;
	volatile LONG m_terminal;
};

#endif //REMOTEUPLOADPLAN_H
