#ifndef FTP_LISTING_TIME_UTILS_H
#define FTP_LISTING_TIME_UTILS_H

#include <windows.h>

// FTP LIST timestamps have no timezone; preserve their wall-clock time for the local client display.
static inline int ftp_listing_local_system_time_to_filetime(const SYSTEMTIME & localTime, FILETIME * fileTime)
{
	if (!fileTime)
		return -1;

	SYSTEMTIME utc{};
	if (!TzSpecificLocalTimeToSystemTime(NULL, &localTime, &utc))
		return -1;

	return SystemTimeToFileTime(&utc, fileTime) ? 0 : -1;
}

#endif //FTP_LISTING_TIME_UTILS_H
