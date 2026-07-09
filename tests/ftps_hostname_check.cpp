#include "src/FTPSHostnameVerifier.h"

#include <assert.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

static void add_san(X509 *cert, const char *value)
{
	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);

	X509_EXTENSION *ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_alt_name, const_cast<char *>(value));
	assert(ext != NULL);
	assert(X509_add_ext(cert, ext, -1) == 1);
	X509_EXTENSION_free(ext);
}

int main()
{
	X509 *cert = X509_new();
	assert(cert != NULL);
	add_san(cert, "DNS:good.example");

	assert(FTPSCertificateMatchesHostname(cert, "good.example"));
	assert(!FTPSCertificateMatchesHostname(cert, "bad.example"));
	assert(!FTPSCertificateMatchesHostname(cert, ""));
	assert(!FTPSCertificateMatchesHostname(NULL, "good.example"));

	X509_free(cert);

	X509 *ipcert = X509_new();
	assert(ipcert != NULL);
	add_san(ipcert, "IP:127.0.0.1");

	assert(FTPSCertificateMatchesHostname(ipcert, "127.0.0.1"));
	assert(!FTPSCertificateMatchesHostname(ipcert, "127.0.0.2"));

	X509_free(ipcert);
	return 0;
}
