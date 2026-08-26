#include "src/ftp_listing_time_utils.h"

#include <stdio.h>

static int Check(bool condition, const char * message)
{
	if (condition)
		return 0;
	printf("ftp_listing_time_failure=%s\n", message);
	return 1;
}

static bool SameMinute(const SYSTEMTIME & left, const SYSTEMTIME & right)
{
	return left.wYear == right.wYear &&
		left.wMonth == right.wMonth &&
		left.wDay == right.wDay &&
		left.wHour == right.wHour &&
		left.wMinute == right.wMinute;
}

int main()
{
	SYSTEMTIME listed{};
	listed.wYear = 2026;
	listed.wMonth = 8;
	listed.wDay = 26;
	listed.wHour = 14;
	listed.wMinute = 35;

	FILETIME modified{};
	if (Check(ftp_listing_local_system_time_to_filetime(listed, &modified) == 0, "listing time converts to file time")) return 1;
	if (Check(ftp_listing_local_system_time_to_filetime(listed, NULL) == -1, "null file-time output is rejected")) return 1;

	SYSTEMTIME utc{};
	SYSTEMTIME displayed{};
	if (Check(FileTimeToSystemTime(&modified, &utc) != FALSE, "file time converts to UTC")) return 1;
	if (Check(SystemTimeToTzSpecificLocalTime(NULL, &utc, &displayed) != FALSE, "UTC converts to local display time")) return 1;
	if (Check(SameMinute(listed, displayed), "FTP listing wall-clock time does not gain a timezone offset")) return 1;

	printf("ftp_listing_time_exit=0\n");
	return 0;
}
