/*
	NppFTP: FTP/SFTP functionality for Notepad++
	Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FTPSHOSTNAMEVERIFIER_H
#define FTPSHOSTNAMEVERIFIER_H

#include <openssl/x509v3.h>

inline bool FTPSCertificateMatchesHostname(const X509 *certificate, const char *hostname)
{
	if (!certificate || !hostname || hostname[0] == '\0')
		return false;

	if (X509_check_host(certificate, hostname, 0, 0, NULL) == 1)
		return true;

	return X509_check_ip_asc(certificate, hostname, 0) == 1;
}

#endif // FTPSHOSTNAMEVERIFIER_H
