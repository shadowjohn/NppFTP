#include "src/remote_browser_utils.h"

#include <assert.h>
#include <commctrl.h>
#include <stdio.h>

static bool receivedF2 = false;
static bool receivedListEnter = false;
static bool receivedEnter = false;

static LRESULT CALLBACK ParentProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NOTIFY) {
		NMHDR * header = (NMHDR*)lParam;
		if (header->code == LVN_KEYDOWN && ((NMLVKEYDOWN*)lParam)->wVKey == VK_F2)
			receivedF2 = true;
		if (header->code == NM_RETURN)
			receivedListEnter = true;
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

static LRESULT CALLBACK ComboEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	if (message == WM_KEYDOWN && wParam == VK_RETURN)
		receivedEnter = true;

	LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
	MSG * key = (MSG*)lParam;
	if (message == WM_GETDLGCODE && key && key->message == WM_KEYDOWN && key->wParam == VK_RETURN)
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
	LVITEM item{};
	item.mask = LVIF_TEXT;
	item.pszText = (TCHAR*)TEXT("folder");
	assert(ListView_InsertItem(list, &item) == 0);
	item.iItem = 1;
	item.pszText = (TCHAR*)TEXT("file.txt");
	assert(ListView_InsertItem(list, &item) == 1);
	ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	assert(remote_browser_focus_list_item(list, 1) == 0);
	assert(ListView_GetNextItem(list, -1, LVNI_SELECTED) == 1);
	assert((ListView_GetItemState(list, 1, LVIS_FOCUSED) & LVIS_FOCUSED) != 0);
	assert(ListView_GetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED) == 0);
	SetFocus(list);

	MSG key{};
	key.hwnd = list;
	key.message = WM_KEYDOWN;
	key.wParam = VK_F2;
	assert(IsDialogMessage(parent, &key));
	assert(receivedF2);
	key.wParam = VK_RETURN;
	assert(IsDialogMessage(parent, &key));
	assert(receivedListEnter);

	HWND combo = CreateWindow(TEXT("COMBOBOX"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN,
		0, 0, 200, 120, parent, NULL, windowClass.hInstance, NULL);
	assert(combo);
	HWND comboEdit = remote_browser_combo_edit(combo);
	assert(comboEdit);
	assert(comboEdit != combo);
	assert(SetWindowSubclass(comboEdit, ComboEditProc, 1, 0));
	SetFocus(comboEdit);

	MSG enter{};
	enter.hwnd = comboEdit;
	enter.message = WM_KEYDOWN;
	enter.wParam = VK_RETURN;
	assert(IsDialogMessage(parent, &enter));
	assert(receivedEnter);

	DestroyWindow(parent);
	printf("remote_list_keyboard_exit=0\n");
	return 0;
}
