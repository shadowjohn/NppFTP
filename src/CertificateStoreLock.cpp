/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#include <windows.h>

#include "CertificateStoreLock.h"

namespace {
	class CertificateStoreLockState {
	public:
		CertificateStoreLockState() {
			::InitializeCriticalSection(&section);
		}

		CRITICAL_SECTION section;
	};

	CertificateStoreLockState & GetCertificateStoreLockState() {
		// Keep the lock alive through global NppFTP teardown during DLL unload.
		static CertificateStoreLockState * state = new CertificateStoreLockState();
		return *state;
	}
}

CertificateStoreLock::CertificateStoreLock() {
	::EnterCriticalSection(&GetCertificateStoreLockState().section);
}

CertificateStoreLock::~CertificateStoreLock() {
	::LeaveCriticalSection(&GetCertificateStoreLockState().section);
}
