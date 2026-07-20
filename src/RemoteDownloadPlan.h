#ifndef REMOTEDOWNLOADPLAN_H
#define REMOTEDOWNLOADPLAN_H

#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>

#include "FTPFile.h"

struct RemoteDownloadItem {
	bool isDirectory;
	bool isLink;
	bool selected;
	bool scanned;
	std::basic_string<TCHAR> localPath;
	std::string remotePath;
};

class RemoteDownloadPlan {
public:
	int Build(const TCHAR * localParent, const char * remoteRoot);
	int ApplyRemoteDirectoryListing(const char * directoryPath, const FTPFile * files, int count);
	int PrepareLocalDirectories();
	void GetLocalFileCollisions(std::vector<size_t> & indices) const;
	const std::vector<RemoteDownloadItem> & GetItems() const;
	std::vector<RemoteDownloadItem> & GetItems();
	const std::vector<std::basic_string<TCHAR> > & GetFailures() const;

private:
	int AddItem(bool isDirectory, bool isLink, const char * remotePath);
	int GetLocalPath(const char * remotePath, std::basic_string<TCHAR> & localPath) const;
	void AddFailure(const TCHAR * message);

	std::vector<RemoteDownloadItem> m_items;
	std::vector<std::basic_string<TCHAR> > m_failures;
	std::basic_string<TCHAR> m_localRoot;
	std::string m_remoteRoot;
};

#endif //REMOTEDOWNLOADPLAN_H
