#ifndef FILE_SIZE_UTILS_H
#define FILE_SIZE_UTILS_H

#include <windows.h>

static inline float file_size_progress_percent(LONGLONG current, LONGLONG total)
{
	if (total <= 0)
		return -1.0f;
	return (float)((double)current / (double)total * 100.0);
}

static inline ULARGE_INTEGER file_size_parts(LONGLONG size)
{
	ULARGE_INTEGER value{};
	value.QuadPart = size < 0 ? 0 : (ULONGLONG)size;
	return value;
}

#endif //FILE_SIZE_UTILS_H
