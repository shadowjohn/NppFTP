#include "src/remote_browser_utils.h"

#include <assert.h>
#include <commctrl.h>
#include <stdio.h>

static bool receivedF2 = false;

static LRESULT CALLBACK ParentProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NOTIFY) {
		NMHDR * header = (NMHDR*)lParam;
		if (header->code == LVN_KEYDOWN && ((NMLVKEYDOWN*)lParam)->wVKey == VK_F2)
			receivedF2 = true;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK ListProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
	if (message == WM_GETDLGCODE && remote_browser_wants_key((MSG*)lParam))
		result |= DLGC_WANTALLKEYS;
	return result;
}

int main()
{
	INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_LISTVIEW_CLASSES };
	assert(InitCommonControlsEx(&controls));

	WNDCLASS windowClass{};
	windowClass.lpfnWndProc = ParentProc;
	windowClass.hInstance = GetModuleHandle(NULL);
	windowClass.lpszClassName = TEXT("NppFTPRemoteListKeyboardTest");
	assert(RegisterClass(&windowClass));

	HWND parent = CreateWindow(windowClass.lpszClassName, TEXT(""), WS_OVERLAPPED,
		0, 0, 200, 200, NULL, NULL, windowClass.hInstance, NULL);
	assert(parent);
	HWND list = CreateWindow(WC_LISTVIEW, TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT,
		0, 0, 200, 200, parent, NULL, windowClass.hInstance, NULL);
	assert(list);
	assert(SetWindowSubclass(list, ListProc, 1, 0));
	SetFocus(list);

	MSG key{};
	key.hwnd = list;
	key.message = WM_KEYDOWN;
	key.wParam = VK_F2;
	assert(IsDialogMessage(parent, &key));
	assert(receivedF2);

	DestroyWindow(parent);
	printf("remote_list_keyboard_exit=0\n");
	return 0;
}
