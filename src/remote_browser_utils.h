#ifndef REMOTE_BROWSER_UTILS_H
#define REMOTE_BROWSER_UTILS_H

#include <ctype.h>
#include <shlwapi.h>
#include <stdio.h>
#include <tchar.h>
#include <wctype.h>

static inline bool remote_browser_matches_filter(const TCHAR *name, const TCHAR *filter)
{
	if (!name)
		return false;
	if (!filter || filter[0] == 0)
		return true;
	return StrStrI(name, filter) != NULL;
}

static inline void remote_browser_clear_text(TCHAR *buffer, size_t bufferCount)
{
	if (buffer && bufferCount > 0)
		buffer[0] = 0;
}

static inline void remote_browser_format_size(bool isDir, long size, TCHAR *buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0 || isDir || size < 0)
		return;

#ifdef _MSC_VER
	_sntprintf_s(buffer, bufferCount, _TRUNCATE, TEXT("%ld"), size);
#else
	_sntprintf(buffer, bufferCount, TEXT("%ld"), size);
	buffer[bufferCount - 1] = 0;
#endif
}

static inline void remote_browser_type_text(bool isDir, bool isLink, const TCHAR *name, TCHAR *buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0 || isDir)
		return;

	const TCHAR *extension = name ? PathFindExtension(name) : NULL;
	if (extension && extension[0] == TEXT('.') && extension[1] != 0 && extension != name) {
		size_t i = 0;
		for (extension++; extension[i] && i + 1 < bufferCount; i++) {
#ifdef UNICODE
			buffer[i] = (TCHAR)towupper(extension[i]);
#else
			buffer[i] = (TCHAR)toupper((unsigned char)extension[i]);
#endif
		}
		buffer[i] = 0;
		return;
	}

	if (isLink) {
#ifdef _MSC_VER
		_sntprintf_s(buffer, bufferCount, _TRUNCATE, TEXT("LINK"));
#else
		_sntprintf(buffer, bufferCount, TEXT("LINK"));
		buffer[bufferCount - 1] = 0;
#endif
	}
}

static inline void remote_browser_permission_text(const char *mod, TCHAR *buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0 || !mod)
		return;

	size_t i = 0;
	for (; mod[i] && i + 1 < bufferCount; i++)
		buffer[i] = (TCHAR)(unsigned char)mod[i];
	buffer[i] = 0;
}

#endif //REMOTE_BROWSER_UTILS_H
