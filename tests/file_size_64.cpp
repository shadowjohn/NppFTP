#include <windows.h>
#include <vector>
#include "UTCP/include/ftp_c.h"
#include "UTCP/include/utstrlst.h"
#include "src/FTPFile.h"
#include "src/FileObject.h"

#include <assert.h>
#include <stdio.h>
#include <type_traits>

int main()
{
	static_assert(sizeof(((CUT_DIRINFOA*)0)->fileSize) == sizeof(LONGLONG), "CUT_DIRINFOA size must be 64-bit");
	static_assert(sizeof(((FTPFile*)0)->fileSize) == sizeof(LONGLONG), "FTPFile size must be 64-bit");
	static_assert(std::is_same<decltype(((FileObject*)0)->GetSize()), LONGLONG>::value,
		"FileObject::GetSize must return 64-bit");

	LONGLONG value = 0;
	assert(CUT_StrMethods::ParseString("file 5368709120", " ", 1, &value) == UTE_SUCCESS);
	assert(value == 5368709120LL);
	assert(CUT_StrMethods::ParseString(L"file 1099511627776", L" ", 1, &value) == UTE_SUCCESS);
	assert(value == 1099511627776LL);

	FTPFile file{};
	file.fileSize = 5368709120LL;
	assert(file.fileSize == 5368709120LL);

	printf("file_size_64_exit=0\n");
	return 0;
}
