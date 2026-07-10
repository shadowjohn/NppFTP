#ifndef CHMODDIALOG_H
#define CHMODDIALOG_H

#include "Dialog.h"

class ChmodDialog : public Dialog {
public:
	ChmodDialog();

	int Create(HWND parent, const char *currentMode);
	const TCHAR *GetMode() const;

protected:
	using Dialog::Create;

	INT_PTR DlgMsgProc(UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR OnInitDialog();
	void SyncModeFromChecks();

	TCHAR m_mode[4];
};

#endif //CHMODDIALOG_H
