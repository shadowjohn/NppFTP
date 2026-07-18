#ifndef REMOTE_BROWSER_UTILS_H
#define REMOTE_BROWSER_UTILS_H

#include <ctype.h>
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <wctype.h>

enum RemoteOverwriteDecision {
	RemoteOverwriteCancel = 0,
	RemoteOverwriteOverwrite = 1,
	RemoteOverwriteSkip = 2
};

static inline bool remote_browser_wants_key(const MSG *message)
{
	return message && message->message == WM_KEYDOWN &&
		(message->wParam == VK_F2 || message->wParam == VK_RETURN || message->wParam == VK_BACK);
}

static inline bool remote_browser_directory_result_succeeded(int result)
{
	return result != -1;
}

static inline bool remote_browser_create_uses_selected_directory(bool hasSelection, bool isDirectory)
{
	return hasSelection && isDirectory;
}

static inline bool remote_browser_pending_path_matches(const char *pendingPath, const char *completedPath)
{
	return pendingPath && pendingPath[0] && completedPath && strcmp(pendingPath, completedPath) == 0;
}

static inline bool remote_browser_completed_request_commits_pending(bool isFinalTarget, const char *pendingPath, const char *completedRequestPath)
{
	return isFinalTarget && remote_browser_pending_path_matches(pendingPath, completedRequestPath);
}

static inline bool remote_browser_refresh_updates_visible_ui(bool isFinalTarget, const char *pendingPath,
	const char *completedRequestPath, bool isCurrentRemoteDir)
{
	return isFinalTarget && completedRequestPath &&
		(remote_browser_pending_path_matches(pendingPath, completedRequestPath) ||
			(isCurrentRemoteDir && (!pendingPath || pendingPath[0] == 0)));
}

static inline HWND remote_browser_combo_edit(HWND combo)
{
	COMBOBOXINFO info{};
	info.cbSize = sizeof(info);
	return combo && GetComboBoxInfo(combo, &info) ? info.hwndItem : NULL;
}

static inline int remote_browser_focus_list_item(HWND list, int item)
{
	if (!list || item < 0)
		return -1;
	ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	ListView_SetItemState(list, item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	ListView_EnsureVisible(list, item, FALSE);
	return 0;
}

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

static inline void remote_browser_format_system_time(const SYSTEMTIME &value, TCHAR *buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0)
		return;

#ifdef _MSC_VER
	_sntprintf_s(buffer, bufferCount, _TRUNCATE, TEXT("%04u-%02u-%02u %02u:%02u:%02u"),
		value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond);
#else
	_sntprintf(buffer, bufferCount, TEXT("%04u-%02u-%02u %02u:%02u:%02u"),
		value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond);
	buffer[bufferCount - 1] = 0;
#endif
}

static inline void remote_browser_format_size(bool isDir, LONGLONG size, TCHAR *buffer, size_t bufferCount)
{
	remote_browser_clear_text(buffer, bufferCount);
	if (!buffer || bufferCount == 0 || isDir || size < 0)
		return;

	static const TCHAR *units[] = { TEXT("B"), TEXT("KB"), TEXT("MB"), TEXT("GB"), TEXT("TB") };
	double value = (double)size;
	int unit = 0;
	while (value >= 1024.0 && unit < 4) {
		value /= 1024.0;
		unit++;
	}

#ifdef _MSC_VER
	_sntprintf_s(buffer, bufferCount, _TRUNCATE, TEXT("%.2f %s"), value, units[unit]);
#else
	_sntprintf(buffer, bufferCount, TEXT("%.2f %s"), value, units[unit]);
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

static inline int remote_browser_permission_to_octal(const char *mod, TCHAR *mode, size_t modeCount)
{
	if (!mod || !mode || modeCount < 4 || strlen(mod) < 10)
		return -1;

	for (int group = 0; group < 3; group++) {
		int value = 0;
		if (mod[1 + group * 3] == 'r')
			value |= 4;
		if (mod[2 + group * 3] == 'w')
			value |= 2;
		char execute = mod[3 + group * 3];
		if (execute == 'x' || execute == 's' || execute == 't')
			value |= 1;
		mode[group] = (TCHAR)(TEXT('0') + value);
	}
	mode[3] = 0;
	return 0;
}

static inline int remote_browser_octal_to_checks(const TCHAR *mode, bool checks[9])
{
	if (!mode || !checks || mode[0] == 0 || mode[1] == 0 || mode[2] == 0 || mode[3] != 0)
		return -1;

	for (int group = 0; group < 3; group++) {
		if (mode[group] < TEXT('0') || mode[group] > TEXT('7'))
			return -1;
		int value = mode[group] - TEXT('0');
		checks[group * 3] = (value & 4) != 0;
		checks[group * 3 + 1] = (value & 2) != 0;
		checks[group * 3 + 2] = (value & 1) != 0;
	}
	return 0;
}

static inline int remote_browser_checks_to_octal(const bool checks[9], TCHAR *mode, size_t modeCount)
{
	if (!checks || !mode || modeCount < 4)
		return -1;

	for (int group = 0; group < 3; group++) {
		int value = (checks[group * 3] ? 4 : 0) |
			(checks[group * 3 + 1] ? 2 : 0) |
			(checks[group * 3 + 2] ? 1 : 0);
		mode[group] = (TCHAR)(TEXT('0') + value);
	}
	mode[3] = 0;
	return 0;
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
