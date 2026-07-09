#include "src/StdInc.h"
#include "src/Encryption.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

// Mock standard output functions if required
Output* _MainOutput = NULL;
HWND _MainOutputWindow = NULL;
TCHAR * _ConfigPath = NULL;

int OutputDebug(const TCHAR * /*msg*/, ...) { return 0; }
int OutputMsg(const TCHAR * /*msg*/, ...) { return 0; }
int OutputClnt(const TCHAR * /*msg*/, ...) { return 0; }
int OutputErr(const TCHAR * /*msg*/, ...) { return 0; }
int MessageBoxOutput(const TCHAR * /*msg*/) { return 0; }

int main()
{
	Encryption::Init();

	const char * secret = "NppFTP-Super-Secret-Password-1234!";

	// Test 1: Default configuration should use DPAPI
	assert(Encryption::IsDefaultKey());

	char * encrypted = Encryption::Encrypt(NULL, -1, secret, -1);
	assert(encrypted != NULL);

	// Should be prefixed with "DPAPI:"
	assert(strncmp(encrypted, "DPAPI:", 6) == 0);

	char * decrypted = Encryption::Decrypt(NULL, -1, encrypted, true);
	assert(decrypted != NULL);
	assert(strcmp(decrypted, secret) == 0);

	Encryption::FreeData(decrypted);

	char * empty_encrypted = Encryption::Encrypt(NULL, -1, "", -1);
	assert(empty_encrypted != NULL);
	assert(strncmp(empty_encrypted, "DPAPI:", 6) == 0);

	char * empty_decrypted = Encryption::Decrypt(NULL, -1, empty_encrypted, true);
	assert(empty_decrypted != NULL);
	assert(strcmp(empty_decrypted, "") == 0);

	Encryption::FreeData(empty_decrypted);
	Encryption::FreeData(empty_encrypted);

	// Test 2: Backward compatibility test.
	// Encrypt a string using the default key "NppFTP00" but bypass DPAPI by passing key explicitly.
	char * des_encrypted = Encryption::Encrypt("NppFTP00", 8, secret, -1);
	assert(des_encrypted != NULL);
	// DES encryption should NOT have "DPAPI:" prefix
	assert(strncmp(des_encrypted, "DPAPI:", 6) != 0);

	// Decrypt it passing NULL as the key. Since it has no "DPAPI:" prefix, it must fall back to default DES decryption
	char * des_decrypted = Encryption::Decrypt(NULL, -1, des_encrypted, true);
	assert(des_decrypted != NULL);
	assert(strcmp(des_decrypted, secret) == 0);

	Encryption::FreeData(des_decrypted);
	Encryption::FreeData(des_encrypted);
	Encryption::FreeData(encrypted);

	// Test 3: Master Password mode. When a master password is set, we use DES.
	Encryption::SetDefaultKey("MyMaster");
	assert(!Encryption::IsDefaultKey());

	char * master_encrypted = Encryption::Encrypt(NULL, -1, secret, -1);
	assert(master_encrypted != NULL);
	// Master password encryption should NOT have "DPAPI:" prefix
	assert(strncmp(master_encrypted, "DPAPI:", 6) != 0);

	char * master_decrypted = Encryption::Decrypt(NULL, -1, master_encrypted, true);
	assert(master_decrypted != NULL);
	assert(strcmp(master_decrypted, secret) == 0);

	Encryption::FreeData(master_decrypted);
	Encryption::FreeData(master_encrypted);

	Encryption::Deinit();

	printf("encryption_dpapi_exit=0\n");
	return 0;
}
