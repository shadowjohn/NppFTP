#ifndef OVERWRITEDIALOG_H
#define OVERWRITEDIALOG_H

#include "Dialog.h"
#include "remote_browser_utils.h"

class OverwriteDialog : public Dialog {
public:
	OverwriteDialog();
	~OverwriteDialog();

	int Create(HWND parent, const TCHAR *remoteName);
	RemoteOverwriteDecision GetDecision() const;
	bool GetApplyToAll() const;
	bool GetOverwriteAll() const;

protected:
	using Dialog::Create;

	INT_PTR DlgMsgProc(UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR OnInitDialog();

	TCHAR *m_remoteName;
	RemoteOverwriteDecision m_decision;
	bool m_applyToAll;
};

#endif //OVERWRITEDIALOG_H
