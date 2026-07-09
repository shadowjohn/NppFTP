#include "src/StdInc.h"
#include "src/PathUtils.h"

#include <assert.h>

Output* _MainOutput = NULL;
HWND _MainOutputWindow = NULL;
TCHAR * _ConfigPath = NULL;

int OutputDebug(const TCHAR * /*msg*/, ...) { return 0; }
int OutputMsg(const TCHAR * /*msg*/, ...) { return 0; }
int OutputClnt(const TCHAR * /*msg*/, ...) { return 0; }
int OutputErr(const TCHAR * /*msg*/, ...) { return 0; }
int MessageBoxOutput(const TCHAR * /*msg*/) { return 0; }

int main()
{
	TCHAR local[MAX_PATH];

	assert(PU::ConcatExternalToLocal(TEXT("C:\\cache\\root"), "folder/file.txt", local, MAX_PATH) == 0);
	assert(lstrcmp(local, TEXT("C:\\cache\\root\\folder\\file.txt")) == 0);

	assert(PU::ConcatExternalToLocal(TEXT("C:\\cache\\root"), "../escape.txt", local, MAX_PATH) == -1);
	assert(PU::ConcatExternalToLocal(TEXT("C:\\cache\\root"), "folder/../../escape.txt", local, MAX_PATH) == -1);

	return 0;
}
