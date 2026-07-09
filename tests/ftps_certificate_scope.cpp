#include "src/FTPSCertificateScope.h"

#include <assert.h>

int main()
{
	FTPSCertificateScope expected = {"site", "group", "ftp.example", 21, 2};

	assert(FTPSCertificateScopeMatches(expected, expected));

	FTPSCertificateScope wrongProfile = {"other", "group", "ftp.example", 21, 2};
	assert(!FTPSCertificateScopeMatches(expected, wrongProfile));

	FTPSCertificateScope wrongParent = {"site", "other", "ftp.example", 21, 2};
	assert(!FTPSCertificateScopeMatches(expected, wrongParent));

	FTPSCertificateScope wrongHost = {"site", "group", "other.example", 21, 2};
	assert(!FTPSCertificateScopeMatches(expected, wrongHost));

	FTPSCertificateScope wrongPort = {"site", "group", "ftp.example", 990, 2};
	assert(!FTPSCertificateScopeMatches(expected, wrongPort));

	FTPSCertificateScope wrongMode = {"site", "group", "ftp.example", 21, 1};
	assert(!FTPSCertificateScopeMatches(expected, wrongMode));

	FTPSCertificateScope emptyHost = {"site", "group", "", 21, 2};
	assert(!FTPSCertificateScopeMatches(expected, emptyHost));

	return 0;
}
