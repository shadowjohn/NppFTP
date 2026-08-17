/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#ifndef CERTIFICATESTORELOCK_H
#define CERTIFICATESTORELOCK_H

class CertificateStoreLock {
public:
	CertificateStoreLock();
	~CertificateStoreLock();

private:
	CertificateStoreLock(const CertificateStoreLock &);
	CertificateStoreLock & operator=(const CertificateStoreLock &);
};

#endif //CERTIFICATESTORELOCK_H
