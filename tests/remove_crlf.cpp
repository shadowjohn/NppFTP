#include "UTCP/include/stdafx.h"
#include "UTCP/include/utstrlst.h"

#include <assert.h>
#include <string.h>
#include <wchar.h>

int main()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	BYTE *guardBase = (BYTE*)VirtualAlloc(NULL, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	assert(guardBase != NULL);
	DWORD oldProtect = 0;
	assert(VirtualProtect(guardBase, info.dwPageSize, PAGE_NOACCESS, &oldProtect));

	char *empty = (char*)(guardBase + info.dwPageSize);
	empty[0] = '\0';
	char plain[] = "abc";
	char newline[] = "abc\n";
	char crlf[] = "abc\r\n";
	wchar_t *wideEmpty = (wchar_t*)(guardBase + info.dwPageSize);
	wchar_t widePlain[] = L"abc";
	wchar_t wideCrlf[] = L"abc\r\n";

	CUT_StrMethods::RemoveCRLF(empty);
	assert(strcmp(empty, "") == 0);
	assert(CUT_StrMethods::IsWithCRLF(empty) == FALSE);

	CUT_StrMethods::RemoveCRLF(plain);
	assert(strcmp(plain, "abc") == 0);
	assert(CUT_StrMethods::IsWithCRLF(plain) == FALSE);

	CUT_StrMethods::RemoveCRLF(newline);
	assert(strcmp(newline, "abc") == 0);

	CUT_StrMethods::RemoveCRLF(crlf);
	assert(strcmp(crlf, "abc") == 0);

	CUT_StrMethods::RemoveCRLF(wideEmpty);
	assert(wcscmp(wideEmpty, L"") == 0);
	assert(CUT_StrMethods::IsWithCRLF(wideEmpty) == FALSE);

	CUT_StrMethods::RemoveCRLF(widePlain);
	assert(wcscmp(widePlain, L"abc") == 0);
	assert(CUT_StrMethods::IsWithCRLF(widePlain) == FALSE);

	CUT_StrMethods::RemoveCRLF(wideCrlf);
	assert(wcscmp(wideCrlf, L"abc") == 0);

	assert(VirtualFree(guardBase, 0, MEM_RELEASE));
	return 0;
}
