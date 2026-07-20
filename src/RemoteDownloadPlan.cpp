#include "RemoteDownloadPlan.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <string.h>

#if _WIN32_WINNT < 0x0600
extern "C" WINBASEAPI int WINAPI CompareStringOrdinal(LPCWCH left, int leftCount, LPCWCH right, int rightCount, BOOL ignoreCase);
#endif

static bool IsReservedDeviceName(const TCHAR * name)
{
	const TCHAR * extension = _tcschr(name, TEXT('.'));
	int length = extension ? (int)(extension - name) : lstrlen(name);
	if (length == 3 && (_tcsnicmp(name, TEXT("CON"), 3) == 0 || _tcsnicmp(name, TEXT("PRN"), 3) == 0 ||
		_tcsnicmp(name, TEXT("AUX"), 3) == 0 || _tcsnicmp(name, TEXT("NUL"), 3) == 0))
		return true;
	return length == 4 && (name[3] >= TEXT('1') && name[3] <= TEXT('9')) &&
		(_tcsnicmp(name, TEXT("COM"), 3) == 0 || _tcsnicmp(name, TEXT("LPT"), 3) == 0);
}

static bool IsLocalNameValid(const TCHAR * name)
{
	if (!name || !name[0] || lstrcmp(name, TEXT(".")) == 0 || lstrcmp(name, TEXT("..")) == 0)
		return false;

	int length = lstrlen(name);
	if (length >= MAX_PATH || name[length - 1] == TEXT('.') || name[length - 1] == TEXT(' '))
		return false;
	if (IsReservedDeviceName(name))
		return false;

	for (int i = 0; i < length; ++i) {
		if (name[i] < TEXT(' ') || _tcschr(TEXT("\\\\/:*?\"<>|"), name[i]))
			return false;
	}
	return true;
}

static int Utf8SegmentToLocal(const std::string & segment, std::basic_string<TCHAR> & localName)
{
	localName.clear();
	if (segment.empty() || segment.size() >= MAX_PATH || segment == "." || segment == ".." ||
		segment.find('/') != std::string::npos || segment.find('\\') != std::string::npos)
		return -1;

	int wideSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, segment.c_str(), -1, NULL, 0);
	if (wideSize <= 1 || wideSize >= MAX_PATH)
		return -1;
	std::vector<WCHAR> wideBuffer(wideSize, 0);
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, segment.c_str(), -1, &wideBuffer[0], wideSize))
		return -1;

#ifdef UNICODE
	localName.assign(&wideBuffer[0]);
#else
	BOOL usedDefault = FALSE;
	int localSize = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, &wideBuffer[0], -1, NULL, 0, NULL, &usedDefault);
	if (localSize <= 1 || localSize >= MAX_PATH || usedDefault)
		return -1;
	std::vector<TCHAR> localBuffer(localSize, 0);
	usedDefault = FALSE;
	if (!WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, &wideBuffer[0], -1, &localBuffer[0], localSize, NULL, &usedDefault) || usedDefault)
		return -1;
	localName.assign(&localBuffer[0]);
#endif

	return IsLocalNameValid(localName.c_str()) ? 0 : -1;
}

static int NormalizeRemoteDirectory(const char * path, std::string & normalized)
{
	normalized.clear();
	if (!path || path[0] != '/' || strlen(path) >= MAX_PATH)
		return -1;

	normalized = path;
	while (normalized.size() > 1 && normalized[normalized.size() - 1] == '/')
		normalized.erase(normalized.size() - 1);
	if (normalized == "/")
		return -1;

	size_t offset = 1;
	while (offset < normalized.size()) {
		size_t next = normalized.find('/', offset);
		std::string segment = normalized.substr(offset, next == std::string::npos ? std::string::npos : next - offset);
		std::basic_string<TCHAR> localName;
		if (Utf8SegmentToLocal(segment, localName) != 0)
			return -1;
		if (next == std::string::npos)
			break;
		offset = next + 1;
	}
	return 0;
}

static bool IsLocalPathInsideRoot(const TCHAR * root, const TCHAR * path)
{
	int rootLength = lstrlen(root);
	return _tcsnicmp(root, path, rootLength) == 0 && (path[rootLength] == 0 || path[rootLength] == TEXT('\\'));
}

static int JoinLocalPath(const TCHAR * root, const std::vector<std::basic_string<TCHAR> > & segments,
	std::basic_string<TCHAR> & result)
{
	TCHAR current[MAX_PATH]{};
	lstrcpyn(current, root, MAX_PATH);
	for (size_t i = 0; i < segments.size(); ++i) {
		TCHAR combined[MAX_PATH]{};
		if (!PathCombine(combined, current, segments[i].c_str()) || lstrlen(combined) >= MAX_PATH ||
			!IsLocalPathInsideRoot(root, combined))
			return -1;
		lstrcpyn(current, combined, MAX_PATH);
	}
	result = current;
	return 0;
}

static int CreateLocalDirectory(const TCHAR * path)
{
	DWORD attributes = GetFileAttributes(path);
	if (attributes != INVALID_FILE_ATTRIBUTES)
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 ? 0 : -1;

	TCHAR fullPath[MAX_PATH]{};
	DWORD length = GetFullPathName(path, MAX_PATH, fullPath, NULL);
	if (length == 0 || length >= MAX_PATH)
		return -1;
	int result = SHCreateDirectoryEx(NULL, fullPath, NULL);
	if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS)
		return -1;
	attributes = GetFileAttributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
		(attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 ? 0 : -1;
}

static bool HasReparsePointAncestor(const TCHAR * path)
{
	TCHAR current[MAX_PATH]{};
	DWORD length = GetFullPathName(path, MAX_PATH, current, NULL);
	if (length == 0 || length >= MAX_PATH)
		return true;

	for (;;) {
		DWORD attributes = GetFileAttributes(current);
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			return true;
		if (PathIsRoot(current))
			return false;
		if (!PathRemoveFileSpec(current))
			return true;
	}
}

static bool HasExistingReparsePointAncestor(const TCHAR * path)
{
	TCHAR current[MAX_PATH]{};
	DWORD length = GetFullPathName(path, MAX_PATH, current, NULL);
	if (length == 0 || length >= MAX_PATH)
		return true;

	for (;;) {
		DWORD attributes = GetFileAttributes(current);
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			return true;
		if (PathIsRoot(current))
			return false;
		if (!PathRemoveFileSpec(current))
			return true;
	}
}

static bool HasRemotePrefix(const std::string & root, const std::string & path)
{
	return path == root || (path.size() > root.size() && path.compare(0, root.size(), root) == 0 && path[root.size()] == '/');
}

static bool LocalPathsEqualIgnoreCase(const TCHAR * left, const TCHAR * right)
{
#ifdef UNICODE
	return CompareStringOrdinal(left, -1, right, -1, TRUE) == CSTR_EQUAL;
#else
	int leftLength = MultiByteToWideChar(CP_ACP, 0, left, -1, NULL, 0);
	int rightLength = MultiByteToWideChar(CP_ACP, 0, right, -1, NULL, 0);
	if (leftLength <= 0 || rightLength <= 0)
		return false;
	std::vector<WCHAR> leftWide(leftLength, 0);
	std::vector<WCHAR> rightWide(rightLength, 0);
	if (!MultiByteToWideChar(CP_ACP, 0, left, -1, &leftWide[0], leftLength) ||
		!MultiByteToWideChar(CP_ACP, 0, right, -1, &rightWide[0], rightLength))
		return false;
	return CompareStringOrdinal(&leftWide[0], -1, &rightWide[0], -1, TRUE) == CSTR_EQUAL;
#endif
}

int RemoteDownloadPlan::Build(const TCHAR * localParent, const char * remoteRoot)
{
	m_items.clear();
	m_failures.clear();
	m_localRoot.clear();
	m_remoteRoot.clear();
	if (!localParent || !localParent[0] || lstrlen(localParent) >= MAX_PATH ||
		NormalizeRemoteDirectory(remoteRoot, m_remoteRoot) != 0)
		return -1;

	DWORD attributes = GetFileAttributes(localParent);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
		(attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || HasReparsePointAncestor(localParent))
		return -1;

	const char * slash = strrchr(m_remoteRoot.c_str(), '/');
	std::basic_string<TCHAR> rootName;
	if (!slash || Utf8SegmentToLocal(slash + 1, rootName) != 0)
		return -1;

	TCHAR root[MAX_PATH]{};
	if (!PathCombine(root, localParent, rootName.c_str()) || lstrlen(root) >= MAX_PATH)
		return -1;
	attributes = GetFileAttributes(root);
	if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		return -1;
	m_localRoot = root;
	return AddItem(true, false, m_remoteRoot.c_str());
}

int RemoteDownloadPlan::AddItem(bool isDirectory, bool isLink, const char * remotePath)
{
	std::string normalized;
	if (!remotePath || strlen(remotePath) >= MAX_PATH || !HasRemotePrefix(m_remoteRoot, remotePath))
		return -1;

	normalized = remotePath;
	if (isDirectory) {
		while (normalized.size() > m_remoteRoot.size() && normalized[normalized.size() - 1] == '/')
			normalized.erase(normalized.size() - 1);
	}
	if (!HasRemotePrefix(m_remoteRoot, normalized))
		return -1;

	RemoteDownloadItem item{};
	item.isDirectory = isDirectory;
	item.isLink = isLink;
	item.selected = !isLink;
	item.scanned = !isDirectory;
	item.remotePath = normalized;
	if (GetLocalPath(normalized.c_str(), item.localPath) != 0)
		return -1;
	m_items.push_back(item);
	return 0;
}

int RemoteDownloadPlan::GetLocalPath(const char * remotePath, std::basic_string<TCHAR> & localPath) const
{
	if (!remotePath || strlen(remotePath) >= MAX_PATH || !HasRemotePrefix(m_remoteRoot, remotePath))
		return -1;

	std::vector<std::basic_string<TCHAR> > segments;
	std::string path(remotePath);
	if (path.size() > m_remoteRoot.size()) {
		size_t offset = m_remoteRoot.size() + 1;
		while (offset < path.size()) {
			size_t next = path.find('/', offset);
			std::basic_string<TCHAR> localName;
			if (Utf8SegmentToLocal(path.substr(offset, next == std::string::npos ? std::string::npos : next - offset), localName) != 0)
				return -1;
			segments.push_back(localName);
			if (next == std::string::npos)
				break;
			offset = next + 1;
		}
	}
	return JoinLocalPath(m_localRoot.c_str(), segments, localPath);
}

int RemoteDownloadPlan::ApplyRemoteDirectoryListing(const char * directoryPath, const FTPFile * files, int count)
{
	std::string directory;
	if (count < 0 || (count > 0 && !files) || NormalizeRemoteDirectory(directoryPath, directory) != 0 ||
		!HasRemotePrefix(m_remoteRoot, directory))
		return -1;
	std::basic_string<TCHAR> localDirectory;
	if (GetLocalPath(directory.c_str(), localDirectory) != 0) {
		AddFailure(TEXT("Skipped remote directory outside the local root."));
		return 0;
	}
	if (HasExistingReparsePointAncestor(localDirectory.c_str())) {
		AddFailure(TEXT("Skipped remote directory through a local reparse point."));
		return 0;
	}

	for (int i = 0; i < count; ++i) {
		const char * path = files[i].filePath;
		const size_t directoryLength = directory.size();
		if (!path || strncmp(path, directory.c_str(), directoryLength) != 0 || path[directoryLength] != '/') {
			AddFailure(TEXT("Skipped malformed remote download path."));
			continue;
		}

		std::string child(path + directoryLength + 1);
		std::basic_string<TCHAR> localName;
		if (Utf8SegmentToLocal(child, localName) != 0) {
			AddFailure(TEXT("Skipped malformed remote download name."));
			continue;
		}

		std::string remotePath = directory + "/" + child;
		if (files[i].fileType == FTPTypeLink) {
			AddFailure(TEXT("Skipped remote symbolic link."));
			continue;
		}
		if (files[i].fileType != FTPTypeDir && files[i].fileType != FTPTypeFile) {
			AddFailure(TEXT("Skipped unknown remote download item."));
			continue;
		}
		std::basic_string<TCHAR> localPath;
		if (GetLocalPath(remotePath.c_str(), localPath) != 0) {
			AddFailure(TEXT("Skipped remote download path outside the local root."));
			continue;
		}
		bool duplicate = false;
		for (size_t j = 0; j < m_items.size(); ++j) {
			if (LocalPathsEqualIgnoreCase(m_items[j].localPath.c_str(), localPath.c_str())) {
				duplicate = true;
				break;
			}
		}
		if (AddItem(files[i].fileType == FTPTypeDir, false, remotePath.c_str()) != 0) {
			AddFailure(TEXT("Skipped remote download path outside the local root."));
			continue;
		}
		if (duplicate) {
			m_items.back().selected = false;
			AddFailure(TEXT("Skipped remote name conflicting with a local path."));
		}
	}
	return 0;
}

int RemoteDownloadPlan::PrepareLocalDirectories()
{
	for (size_t i = 0; i < m_items.size(); ++i) {
		RemoteDownloadItem & item = m_items[i];
		if (!item.selected || !item.isDirectory)
			continue;
		if (CreateLocalDirectory(item.localPath.c_str()) == 0)
			continue;

		AddFailure((std::basic_string<TCHAR>(TEXT("Local file blocks directory: ")) + item.localPath).c_str());
		for (size_t j = i; j < m_items.size(); ++j) {
			if (m_items[j].localPath == item.localPath ||
				(m_items[j].localPath.size() > item.localPath.size() &&
				m_items[j].localPath.compare(0, item.localPath.size(), item.localPath) == 0 &&
				m_items[j].localPath[item.localPath.size()] == TEXT('\\')))
				m_items[j].selected = false;
		}
	}
	for (size_t i = 0; i < m_items.size(); ++i) {
		RemoteDownloadItem & item = m_items[i];
		if (!item.selected || item.isDirectory)
			continue;
		DWORD attributes = GetFileAttributes(item.localPath.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
			AddFailure((std::basic_string<TCHAR>(TEXT("Local reparse point blocks file: ")) + item.localPath).c_str());
			item.selected = false;
		} else if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			AddFailure((std::basic_string<TCHAR>(TEXT("Local directory blocks file: ")) + item.localPath).c_str());
			item.selected = false;
		}
	}
	return 0;
}

void RemoteDownloadPlan::GetLocalFileCollisions(std::vector<size_t> & indices) const
{
	indices.clear();
	for (size_t i = 0; i < m_items.size(); ++i) {
		const RemoteDownloadItem & item = m_items[i];
		if (!item.selected || item.isDirectory || item.isLink)
			continue;
		DWORD attributes = GetFileAttributes(item.localPath.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			indices.push_back(i);
	}
}

const std::vector<RemoteDownloadItem> & RemoteDownloadPlan::GetItems() const
{
	return m_items;
}

std::vector<RemoteDownloadItem> & RemoteDownloadPlan::GetItems()
{
	return m_items;
}

const std::vector<std::basic_string<TCHAR> > & RemoteDownloadPlan::GetFailures() const
{
	return m_failures;
}

void RemoteDownloadPlan::AddFailure(const TCHAR * message)
{
	m_failures.push_back(message ? message : TEXT("Remote download plan failure."));
}

RemoteDownloadBatch::RemoteDownloadBatch(RemoteDownloadPlan * downloadPlan) :
	plan(downloadPlan),
	completedCount(0),
	m_references(1)
{
}

RemoteDownloadBatch::~RemoteDownloadBatch()
{
	delete plan;
}

void RemoteDownloadBatch::AddRef()
{
	InterlockedIncrement(&m_references);
}

void RemoteDownloadBatch::Release()
{
	if (InterlockedDecrement(&m_references) == 0)
		delete this;
}

void RemoteDownloadBatch::RecordCanceled(const char * remotePath)
{
	canceledPaths.push_back(remotePath ? remotePath : "(unknown path)");
}
