#include "RemoteUploadPlan.h"

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

int RemoteUploadPlan::Build(const TCHAR * localDirectory, const char * remoteParent)
{
	m_items.clear();
	if (!localDirectory || !remoteParent || lstrlen(localDirectory) <= 0 || lstrlen(localDirectory) >= MAX_PATH)
		return -1;

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
		if (item.isDirectory || RemoteParentPath(item.remotePath) != directory)
			continue;
		item.remoteFileExists = false;
		const char * itemName = RemoteFilename(item.remotePath.c_str());
		for (int j = 0; j < count; ++j) {
			if (files[j].fileType == FTPTypeDir)
				continue;
			const char * listedName = RemoteFilename(files[j].filePath);
			if (itemName && listedName && strcmp(itemName, listedName) == 0) {
				item.remoteFileExists = true;
				break;
			}
		}
	}
	return 0;
}

const std::vector<RemoteUploadItem> & RemoteUploadPlan::GetItems() const
{
	return m_items;
}

std::vector<RemoteUploadItem> & RemoteUploadPlan::GetItems()
{
	return m_items;
}
