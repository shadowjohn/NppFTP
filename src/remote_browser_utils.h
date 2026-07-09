#ifndef REMOTE_BROWSER_UTILS_H
#define REMOTE_BROWSER_UTILS_H

#include <ctype.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
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

static inline int remote_browser_simplify_typed_path(const char *path, const char *currentDir, char *buffer, size_t bufferCount)
{
	if (!path || path[0] == 0 || !buffer || bufferCount < 2)
		return -1;
	if (path[0] != '/' && (!currentDir || currentDir[0] != '/'))
		return -1;

	size_t pathLen = strlen(path);
	size_t currentLen = (path[0] == '/') ? 0 : strlen(currentDir);
	size_t combinedLen = pathLen + currentLen + ((path[0] == '/') ? 0 : 1);
	char *combined = new char[combinedLen + 1];
	size_t *marks = new size_t[combinedLen + 1];

	if (path[0] == '/') {
		strcpy(combined, path);
	} else {
		strcpy(combined, currentDir);
		strcat(combined, "/");
		strcat(combined, path);
	}

	buffer[0] = '/';
	buffer[1] = 0;
	size_t outputLen = 1;
	size_t markCount = 0;
	const char *cursor = combined;

	while (*cursor) {
		while (*cursor == '/')
			cursor++;
		const char *segment = cursor;
		while (*cursor && *cursor != '/')
			cursor++;
		size_t segmentLen = cursor - segment;
		if (segmentLen == 0)
			continue;
		if (segmentLen == 1 && segment[0] == '.')
			continue;
		if (segmentLen == 2 && segment[0] == '.' && segment[1] == '.') {
			if (markCount > 0) {
				outputLen = marks[--markCount];
				buffer[outputLen] = 0;
			}
			continue;
		}

		size_t separatorLen = (outputLen > 1) ? 1 : 0;
		if (outputLen + separatorLen + segmentLen >= bufferCount) {
			delete [] marks;
			delete [] combined;
			return -1;
		}
		marks[markCount++] = outputLen;
		if (separatorLen)
			buffer[outputLen++] = '/';
		memcpy(buffer + outputLen, segment, segmentLen);
		outputLen += segmentLen;
		buffer[outputLen] = 0;
	}

	delete [] marks;
	delete [] combined;
	return 0;
}

#endif //REMOTE_BROWSER_UTILS_H
