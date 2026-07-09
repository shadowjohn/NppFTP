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

#ifndef FTPSCERTIFICATESCOPE_H
#define FTPSCERTIFICATESCOPE_H

#include <string.h>

struct FTPSCertificateScope {
	const char * profileName;
	const char * profileParent;
	const char * hostname;
	int port;
	int securityMode;
};

inline bool FTPSCertificateScopeHasText(const char * value) {
	return value && value[0] != '\0';
}

inline bool FTPSCertificateScopeSameText(const char * left, const char * right) {
	return left && right && strcmp(left, right) == 0;
}

inline bool FTPSCertificateScopeIsComplete(const FTPSCertificateScope & scope) {
	return FTPSCertificateScopeHasText(scope.profileName) &&
		scope.profileParent != NULL &&
		FTPSCertificateScopeHasText(scope.hostname) &&
		scope.port > 0 &&
		scope.securityMode >= 0;
}

inline bool FTPSCertificateScopeMatches(const FTPSCertificateScope & expected, const FTPSCertificateScope & actual) {
	return FTPSCertificateScopeIsComplete(expected) &&
		FTPSCertificateScopeIsComplete(actual) &&
		expected.port == actual.port &&
		expected.securityMode == actual.securityMode &&
		FTPSCertificateScopeSameText(expected.profileName, actual.profileName) &&
		FTPSCertificateScopeSameText(expected.profileParent, actual.profileParent) &&
		FTPSCertificateScopeSameText(expected.hostname, actual.hostname);
}

#endif //FTPSCERTIFICATESCOPE_H
