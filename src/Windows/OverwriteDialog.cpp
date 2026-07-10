#include "StdInc.h"
#include "OverwriteDialog.h"

#include "resource.h"
#include <windowsx.h>

OverwriteDialog::OverwriteDialog() :
	Dialog(IDD_DIALOG_OVERWRITE),
	m_remoteName(SU::DupString(TEXT(""))),
	m_decision(RemoteOverwriteCancel),
	m_overwriteAll(false)
{
}

OverwriteDialog::~OverwriteDialog()
{
	SU::FreeTChar(m_remoteName);
}

int OverwriteDialog::Create(HWND parent, const TCHAR *remoteName)
{
	SU::FreeTChar(m_remoteName);
	m_remoteName = SU::DupString(remoteName ? remoteName : TEXT(""));
	m_decision = RemoteOverwriteCancel;
	m_overwriteAll = false;
	return Dialog::Create(parent, true, TEXT("File exists"));
}

RemoteOverwriteDecision OverwriteDialog::GetDecision() const
{
	return m_decision;
}

bool OverwriteDialog::GetOverwriteAll() const
{
	return m_overwriteAll;
}

INT_PTR OverwriteDialog::DlgMsgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_COMMAND) {
		switch (LOWORD(wParam)) {
			case IDC_BUTTON_OVERWRITE:
				m_decision = RemoteOverwriteOverwrite;
				m_overwriteAll = Button_GetCheck(GetDlgItem(m_hwnd, IDC_CHECK_OVERWRITE_ALL)) == BST_CHECKED;
				EndDialog(m_hwnd, 1);
				return TRUE;
			case IDC_BUTTON_SKIP:
				m_decision = RemoteOverwriteSkip;
				m_overwriteAll = false;
				EndDialog(m_hwnd, 1);
				return TRUE;
			case IDB_BUTTON_CANCEL:
				m_decision = RemoteOverwriteCancel;
				m_overwriteAll = false;
				EndDialog(m_hwnd, 2);
				return TRUE;
		}
	}
	return Dialog::DlgMsgProc(message, wParam, lParam);
}

INT_PTR OverwriteDialog::OnInitDialog()
{
	Dialog::OnInitDialog();
	SetDlgItemText(m_hwnd, IDC_STATIC_OVERWRITE_NAME, m_remoteName);
	Button_SetCheck(GetDlgItem(m_hwnd, IDC_CHECK_OVERWRITE_ALL), BST_UNCHECKED);
	return TRUE;
}
