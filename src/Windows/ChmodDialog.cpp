#include "StdInc.h"
#include "ChmodDialog.h"

#include "remote_browser_utils.h"
#include "resource.h"
#include <windowsx.h>

static const int ChmodCheckIds[9] = {
	IDC_CHECK_CHMOD_OWNER_READ,
	IDC_CHECK_CHMOD_OWNER_WRITE,
	IDC_CHECK_CHMOD_OWNER_EXECUTE,
	IDC_CHECK_CHMOD_GROUP_READ,
	IDC_CHECK_CHMOD_GROUP_WRITE,
	IDC_CHECK_CHMOD_GROUP_EXECUTE,
	IDC_CHECK_CHMOD_OTHER_READ,
	IDC_CHECK_CHMOD_OTHER_WRITE,
	IDC_CHECK_CHMOD_OTHER_EXECUTE
};

ChmodDialog::ChmodDialog() :
	Dialog(IDD_DIALOG_CHMOD)
{
	lstrcpy(m_mode, TEXT("755"));
}

int ChmodDialog::Create(HWND parent, const char *currentMode)
{
	if (remote_browser_permission_to_octal(currentMode, m_mode, 4) != 0)
		lstrcpy(m_mode, TEXT("755"));
	return Dialog::Create(parent, true, TEXT("FTP CHMOD"));
}

const TCHAR *ChmodDialog::GetMode() const
{
	return m_mode;
}

INT_PTR ChmodDialog::DlgMsgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_COMMAND) {
		int control = LOWORD(wParam);
		if (control == IDB_BUTTON_CANCEL) {
			EndDialog(m_hwnd, 2);
			return TRUE;
		}
		if (control == IDB_BUTTON_OK) {
			SyncModeFromChecks();
			EndDialog(m_hwnd, 1);
			return TRUE;
		}
		for (int i = 0; i < 9; i++) {
			if (control == ChmodCheckIds[i]) {
				SyncModeFromChecks();
				return TRUE;
			}
		}
	}
	return Dialog::DlgMsgProc(message, wParam, lParam);
}

INT_PTR ChmodDialog::OnInitDialog()
{
	Dialog::OnInitDialog();
	bool checks[9]{};
	remote_browser_octal_to_checks(m_mode, checks);
	for (int i = 0; i < 9; i++)
		Button_SetCheck(GetDlgItem(m_hwnd, ChmodCheckIds[i]), checks[i] ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(m_hwnd, IDC_STATIC_CHMOD_MODE, m_mode);
	return TRUE;
}

void ChmodDialog::SyncModeFromChecks()
{
	bool checks[9]{};
	for (int i = 0; i < 9; i++)
		checks[i] = Button_GetCheck(GetDlgItem(m_hwnd, ChmodCheckIds[i])) == BST_CHECKED;
	remote_browser_checks_to_octal(checks, m_mode, 4);
	SetDlgItemText(m_hwnd, IDC_STATIC_CHMOD_MODE, m_mode);
}
