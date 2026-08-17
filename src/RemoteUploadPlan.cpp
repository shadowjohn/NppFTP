#include "RemoteUploadPlan.h"

#include <algorithm>
#include <shlwapi.h>
#include <string.h>

static int LocalNameToUtf8(const TCHAR * localName, std::string & utf8Name)
{
	utf8Name.clear();
	if (!localName || !localName[0])
		return -1;

#ifdef UNICODE
	int size = WideCharToMultiByte(CP_UTF8, 0, localName, -1, NULL, 0, NULL, NULL);
	if (size <= 1 || size > MAX_PATH)
		return -1;
	std::vector<char> buffer(size, 0);
	if (!WideCharToMultiByte(CP_UTF8, 0, localName, -1, &buffer[0], size, NULL, NULL))
		return -1;
	utf8Name.assign(&buffer[0]);
#else
	utf8Name.assign(localName);
#endif

	return utf8Name.size() < MAX_PATH ? 0 : -1;
}

static int JoinRemotePath(const char * parent, const std::string & name, std::string & result)
{
	if (!parent || !parent[0] || strlen(parent) >= MAX_PATH || name.empty())
		return -1;

	result.assign(parent);
	if (result[result.size() - 1] != '/')
		result.push_back('/');
	result.append(name);
	return result.size() < MAX_PATH ? 0 : -1;
}

static std::string RemoteParentPath(const std::string & path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return std::string();
	if (slash == 0)
		return std::string("/");
	return path.substr(0, slash);
}

static const char * RemoteFilename(const char * path)
{
	if (!path)
		return NULL;
	const char * slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static size_t RemotePathDepth(const std::string & path)
{
	size_t depth = 0;
	bool inPart = false;
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '/') {
			inPart = false;
		} else if (!inPart) {
			++depth;
			inPart = true;
		}
	}
	return depth;
}

RemoteUploadFileOutcome resolve_remote_upload_file_outcome(int result, bool canceled)
{
	if (canceled)
		return RemoteUploadFileCanceled;
	return result == -1 ? RemoteUploadFileFailed : RemoteUploadFileSucceeded;
}

bool should_dispatch_remote_upload_after_prepare(int result, bool canceled)
{
	return result != -1 && !canceled;
}

int RemoteUploadPlan::Build(const TCHAR * localDirectory, const char * remoteParent)
{
	m_items.clear();
	m_targetPath.clear();
	if (!localDirectory || !remoteParent || lstrlen(localDirectory) <= 0 || lstrlen(localDirectory) >= MAX_PATH)
		return -1;
	if (!remoteParent[0] || strlen(remoteParent) >= MAX_PATH)
		return -1;
	m_targetPath = remoteParent;
	while (m_targetPath.size() > 1 && m_targetPath[m_targetPath.size() - 1] == '/')
		m_targetPath.erase(m_targetPath.size() - 1);

	TCHAR cleanDirectory[MAX_PATH]{};
	lstrcpyn(cleanDirectory, localDirectory, MAX_PATH);
	PathRemoveBackslash(cleanDirectory);
	DWORD attributes = GetFileAttributes(cleanDirectory);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
		(attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		return -1;

	const TCHAR * baseName = PathFindFileName(cleanDirectory);
	std::string remoteName;
	std::string remoteDirectory;
	if (!baseName || !baseName[0] || LocalNameToUtf8(baseName, remoteName) != 0 ||
		JoinRemotePath(remoteParent, remoteName, remoteDirectory) != 0)
		return -1;

	if (AddDirectoryRecursive(cleanDirectory, remoteDirectory.c_str()) != 0) {
		m_items.clear();
		return -1;
	}
	return 0;
}

int RemoteUploadPlan::AddDirectory(const TCHAR * localPath, const char * remotePath)
{
	if (!localPath || !localPath[0] || !remotePath || !remotePath[0] ||
		lstrlen(localPath) >= MAX_PATH || strlen(remotePath) >= MAX_PATH)
		return -1;

	RemoteUploadItem item{};
	item.isDirectory = true;
	item.localPath = localPath;
	item.remotePath = remotePath;
	item.remoteDirectoryExists = false;
	item.remoteFileExists = false;
	item.selected = true;

	std::vector<RemoteUploadItem>::iterator position = m_items.end();
	for (std::vector<RemoteUploadItem>::iterator it = m_items.begin(); it != m_items.end(); ++it) {
		if (!it->isDirectory) {
			position = it;
			break;
		}
	}
	m_items.insert(position, item);
	return 0;
}

int RemoteUploadPlan::AddFile(const TCHAR * localPath, const char * remotePath)
{
	if (!localPath || !localPath[0] || !remotePath || !remotePath[0] ||
		lstrlen(localPath) >= MAX_PATH || strlen(remotePath) >= MAX_PATH)
		return -1;

	RemoteUploadItem item{};
	item.isDirectory = false;
	item.localPath = localPath;
	item.remotePath = remotePath;
	item.remoteDirectoryExists = false;
	item.remoteFileExists = false;
	item.selected = true;
	m_items.push_back(item);
	return 0;
}

int RemoteUploadPlan::AddDirectoryRecursive(const TCHAR * localDirectory, const char * remoteDirectory)
{
	if (AddDirectory(localDirectory, remoteDirectory) != 0)
		return -1;

	TCHAR searchPath[MAX_PATH]{};
	if (!PathCombine(searchPath, localDirectory, TEXT("*")))
		return -1;

	WIN32_FIND_DATA findData{};
	HANDLE find = FindFirstFile(searchPath, &findData);
	if (find == INVALID_HANDLE_VALUE)
		return -1;

	int result = 0;
	do {
		if (lstrcmp(findData.cFileName, TEXT(".")) == 0 || lstrcmp(findData.cFileName, TEXT("..")) == 0)
			continue;
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			continue;

		TCHAR localPath[MAX_PATH]{};
		std::string remoteName;
		std::string remotePath;
		if (!PathCombine(localPath, localDirectory, findData.cFileName) ||
			LocalNameToUtf8(findData.cFileName, remoteName) != 0 ||
			JoinRemotePath(remoteDirectory, remoteName, remotePath) != 0) {
			result = -1;
			break;
		}

		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (AddDirectoryRecursive(localPath, remotePath.c_str()) != 0) {
				result = -1;
				break;
			}
		} else if (AddFile(localPath, remotePath.c_str()) != 0) {
			result = -1;
			break;
		}
	} while (FindNextFile(find, &findData));

	if (result == 0 && GetLastError() != ERROR_NO_MORE_FILES)
		result = -1;
	FindClose(find);
	return result;
}

int RemoteUploadPlan::ApplyRemoteDirectoryListing(const char * directoryPath, const FTPFile * files, int count)
{
	if (!directoryPath || count < 0 || (count > 0 && !files))
		return -1;

	std::string directory(directoryPath);
	while (directory.size() > 1 && directory[directory.size() - 1] == '/')
		directory.erase(directory.size() - 1);

	for (size_t i = 0; i < m_items.size(); ++i) {
		RemoteUploadItem & item = m_items[i];
		if (RemoteParentPath(item.remotePath) != directory)
			continue;
		item.remoteDirectoryExists = false;
		item.remoteFileExists = false;
		const char * itemName = RemoteFilename(item.remotePath.c_str());
		for (int j = 0; j < count; ++j) {
			bool listedDirectory = files[j].fileType == FTPTypeDir || files[j].fileType == FTPTypeLink;
			if ((item.isDirectory && !listedDirectory) || (!item.isDirectory && files[j].fileType == FTPTypeDir))
				continue;
			const char * listedName = RemoteFilename(files[j].filePath);
			if (itemName && listedName && strcmp(itemName, listedName) == 0) {
				if (item.isDirectory)
					item.remoteDirectoryExists = true;
				else
					item.remoteFileExists = true;
				break;
			}
		}
	}
	return 0;
}

const std::string & RemoteUploadPlan::GetTargetPath() const
{
	return m_targetPath;
}

const std::vector<RemoteUploadItem> & RemoteUploadPlan::GetItems() const
{
	return m_items;
}

std::vector<RemoteUploadItem> & RemoteUploadPlan::GetItems()
{
	return m_items;
}

std::vector<const RemoteUploadItem*> RemoteUploadPlan::GetDirectoryItems() const
{
	std::vector<const RemoteUploadItem*> directories;
	for (size_t i = 0; i < m_items.size(); ++i) {
		if (m_items[i].isDirectory)
			directories.push_back(&m_items[i]);
	}
	std::stable_sort(directories.begin(), directories.end(), [](const RemoteUploadItem * left, const RemoteUploadItem * right) {
		return RemotePathDepth(left->remotePath) < RemotePathDepth(right->remotePath);
	});
	return directories;
}

std::vector<const RemoteUploadItem*> RemoteUploadPlan::GetSelectedFileItems() const
{
	std::vector<const RemoteUploadItem*> files;
	for (size_t i = 0; i < m_items.size(); ++i) {
		if (!m_items[i].isDirectory && m_items[i].selected)
			files.push_back(&m_items[i]);
	}
	return files;
}

int RemoteUploadPlan::GetSkippedFileCount() const
{
	int skipped = 0;
	for (size_t i = 0; i < m_items.size(); ++i) {
		if (!m_items[i].isDirectory && !m_items[i].selected)
			++skipped;
	}
	return skipped;
}

RemoteUploadBatch::RemoteUploadBatch(RemoteUploadPlan * uploadPlan, const char * refreshPath,
	HWND completionWindow, LONG completionGeneration) :
	plan(uploadPlan),
	targetPath(refreshPath ? refreshPath : ""),
	completedCount(0),
	m_references(1),
	m_selectedFiles(0),
	m_skippedFiles(0),
	m_failedFiles(0),
	m_canceledFiles(0),
	m_remainingFiles(0),
	m_completionRequested(0),
	m_unstartedCancellationRecorded(0),
	m_completionWindow(completionWindow),
	m_completionGeneration(completionGeneration)
{
	InitializeCriticalSection(&m_canceledPathsLock);
}

RemoteUploadBatch::~RemoteUploadBatch()
{
	DeleteCriticalSection(&m_canceledPathsLock);
	delete plan;
}

void RemoteUploadBatch::AddRef()
{
	InterlockedIncrement(&m_references);
}

void RemoteUploadBatch::Release()
{
	if (InterlockedDecrement(&m_references) == 0)
		delete this;
}

void RemoteUploadBatch::InitializeFileCounts(int selectedFiles, int skippedFiles)
{
	InterlockedExchange(&m_selectedFiles, selectedFiles < 0 ? 0 : selectedFiles);
	InterlockedExchange(&m_skippedFiles, skippedFiles < 0 ? 0 : skippedFiles);
	InterlockedExchange(&m_remainingFiles, selectedFiles < 0 ? 0 : selectedFiles);
	InterlockedExchange(&m_completionRequested, 0);
	InterlockedExchange(&m_unstartedCancellationRecorded, 0);
}

void RemoteUploadBatch::RecordFileSucceeded()
{
	InterlockedIncrement(&completedCount);
}

void RemoteUploadBatch::RecordFileFailed()
{
	InterlockedIncrement(&m_failedFiles);
}

void RemoteUploadBatch::RecordCanceled(const char * remotePath)
{
	InterlockedIncrement(&m_canceledFiles);
	EnterCriticalSection(&m_canceledPathsLock);
	m_canceledPaths.push_back(remotePath ? remotePath : "(unknown path)");
	LeaveCriticalSection(&m_canceledPathsLock);
}

bool RemoteUploadBatch::CancelUnstartedSelectedFiles()
{
	if (InterlockedCompareExchange(&m_unstartedCancellationRecorded, 1, 0) != 0)
		return false;

	bool requestCompletion = false;
	std::vector<const RemoteUploadItem*> selectedFiles = plan ? plan->GetSelectedFileItems() : std::vector<const RemoteUploadItem*>();
	for (size_t i = 0; i < selectedFiles.size(); ++i) {
		RecordCanceled(selectedFiles[i]->remotePath.c_str());
		if (CompleteFileTerminal())
			requestCompletion = true;
	}
	if (selectedFiles.empty())
		requestCompletion = RequestCompletionIfReady();
	return requestCompletion;
}

bool RemoteUploadBatch::CompleteFileTerminal()
{
	LONG remaining = InterlockedDecrement(&m_remainingFiles);
	if (remaining < 0) {
		InterlockedIncrement(&m_remainingFiles);
		return false;
	}
	return remaining == 0 && RequestCompletionIfReady();
}

bool RemoteUploadBatch::RequestCompletionIfReady()
{
	if (InterlockedCompareExchange(&m_remainingFiles, 0, 0) != 0)
		return false;
	if (InterlockedCompareExchange(&m_completionRequested, 1, 0) != 0)
		return false;
	if (m_completionWindow) {
		AddRef();
		if (!PostMessage(m_completionWindow, NotifyMessageRemoteUploadBatchComplete,
			static_cast<WPARAM>(m_completionGeneration), reinterpret_cast<LPARAM>(this)))
			Release();
	}
	return true;
}

int RemoteUploadBatch::GetSelectedFileCount() const
{
	return static_cast<int>(InterlockedCompareExchange(const_cast<volatile LONG*>(&m_selectedFiles), 0, 0));
}

int RemoteUploadBatch::GetSkippedFileCount() const
{
	return static_cast<int>(InterlockedCompareExchange(const_cast<volatile LONG*>(&m_skippedFiles), 0, 0));
}

int RemoteUploadBatch::GetFailedFileCount() const
{
	return static_cast<int>(InterlockedCompareExchange(const_cast<volatile LONG*>(&m_failedFiles), 0, 0));
}

int RemoteUploadBatch::GetCanceledFileCount() const
{
	return static_cast<int>(InterlockedCompareExchange(const_cast<volatile LONG*>(&m_canceledFiles), 0, 0));
}

int RemoteUploadBatch::GetRemainingFileCount() const
{
	return static_cast<int>(InterlockedCompareExchange(const_cast<volatile LONG*>(&m_remainingFiles), 0, 0));
}

void RemoteUploadBatch::GetCanceledPaths(std::vector<std::string> & paths) const
{
	EnterCriticalSection(&m_canceledPathsLock);
	paths = m_canceledPaths;
	LeaveCriticalSection(&m_canceledPathsLock);
}

RemoteUploadFileTerminalState::RemoteUploadFileTerminalState(RemoteUploadBatch * batch, const char * remotePath) :
	m_batch(batch),
	m_remotePath(remotePath ? remotePath : "(unknown path)"),
	m_canceled(0),
	m_terminal(0)
{
}

void RemoteUploadFileTerminalState::Cancel()
{
	if (InterlockedCompareExchange(&m_canceled, 1, 0) == 0 && m_batch)
		m_batch->RecordCanceled(m_remotePath.c_str());
}

bool RemoteUploadFileTerminalState::Complete()
{
	if (InterlockedCompareExchange(&m_terminal, 1, 0) != 0 || !m_batch)
		return false;
	return m_batch->CompleteFileTerminal();
}

bool RemoteUploadFileTerminalState::WasCanceled() const
{
	return InterlockedCompareExchange(const_cast<volatile LONG*>(&m_canceled), 0, 0) != 0;
}
