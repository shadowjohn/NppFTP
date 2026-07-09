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

#include "StdInc.h"
#include "Encryption.h"

#include <limits.h>
#include <openssl/des.h>
#include <wincrypt.h>

//The default key is initialzed to this. It is pretty much the same as saving passwords plaintext,
//though 8 year olds can't read it then
char * Encryption::_DefaultKey = NULL;
bool Encryption::_IsDefaultKey = true;
const char * defaultString = "NppFTP00";	//must be 8 in length
//const size_t Encryption::KeySize = 8;

static bool StringLengthInt(const char * data, int * size) {
	if (!data || !size) {
		return false;
	}

	size_t length = strlen(data);
	if (length > INT_MAX) {
		return false;
	}

	*size = static_cast<int>(length);
	return true;
}

static char* DPAPI_encrypt(const char * data, int size) {
	if (!data || size < 0) {
		return NULL;
	}

	DATA_BLOB data_in;
	data_in.pbData = (BYTE*)data;
	data_in.cbData = static_cast<DWORD>(size);

	DATA_BLOB data_out;
	if (CryptProtectData(&data_in, L"NppFTP Profile Password", NULL, NULL, NULL, 0, &data_out)) {
		if (data_out.cbData > INT_MAX) {
			LocalFree(data_out.pbData);
			return NULL;
		}

		char * hexData = SU::DataToHex((char*)data_out.pbData, static_cast<int>(data_out.cbData));
		LocalFree(data_out.pbData);
		if (hexData) {
			size_t hex_len = strlen(hexData);
			char * result = new char[6 + hex_len + 1];
			strcpy(result, "DPAPI:");
			strcpy(result + 6, hexData);
			delete [] hexData;
			return result;
		}
	}
	return NULL;
}

static char* DPAPI_decrypt(const char * data, bool addZero) {
	if (strncmp(data, "DPAPI:", 6) != 0) {
		return NULL;
	}
	const char * hex_start = data + 6;
	size_t hex_len = strlen(hex_start);
	if (hex_len > INT_MAX) {
		return NULL;
	}

	char * encrdata = SU::HexToData(hex_start, static_cast<int>(hex_len), false);
	if (!encrdata) {
		return NULL;
	}

	DATA_BLOB data_in;
	data_in.pbData = (BYTE*)encrdata;
	data_in.cbData = static_cast<DWORD>(hex_len / 2);

	DATA_BLOB data_out;
	char * decrypted = NULL;
	if (CryptUnprotectData(&data_in, NULL, NULL, NULL, NULL, 0, &data_out)) {
		size_t decryptedSize = static_cast<size_t>(data_out.cbData) + (addZero ? 1 : 0);
		decrypted = new char[decryptedSize];
		if (decrypted) {
			memcpy(decrypted, data_out.pbData, static_cast<size_t>(data_out.cbData));
			if (addZero) {
				decrypted[data_out.cbData] = 0;
			}
		}
		LocalFree(data_out.pbData);
	}
	delete [] encrdata;
	return decrypted;
}

int Encryption::Init() {
	_DefaultKey = new char[KeySize+1];
	strncpy(_DefaultKey, defaultString, KeySize);
	return 0;
}

int Encryption::Deinit() {
	if (_DefaultKey)
		delete [] _DefaultKey;
	_DefaultKey = NULL;
	return 0;
}

char* Encryption::Encrypt(const char * key, int keysize, const char * data, int size) {
	if (size == -1 && !StringLengthInt(data, &size))
		return NULL;

	if (key == NULL && IsDefaultKey()) {
		return DPAPI_encrypt(data, size);
	}

	char * encdata = DES_encrypt(key, keysize, data, size, false, DES_ENCRYPT);
	if (!encdata)
		return NULL;

	char * hexData = SU::DataToHex(encdata, size);
	delete [] encdata;
	return hexData;
}

char* Encryption::Decrypt(const char * key, int keysize, const char * data, bool addZero) {
	if (!data)
		return NULL;

	if (key == NULL && IsDefaultKey() && strncmp(data, "DPAPI:", 6) == 0) {
		char * dpapiDecrypted = DPAPI_decrypt(data, addZero);
		if (dpapiDecrypted) {
			return dpapiDecrypted;
		}
		return NULL;
	}

	int size = 0;
	if (!StringLengthInt(data, &size))
		return NULL;

	char * encrdata = SU::HexToData(data, size, false);
	if (!encrdata)
		return NULL;

	size = size/2;
	char * decdata = DES_encrypt(key, keysize, encrdata, size, addZero, DES_DECRYPT);
	SU::FreeChar(encrdata);

	return decdata;
}

int Encryption::FreeData(char * data) {
	return SU::FreeChar(data);
}

int Encryption::SetDefaultKey(const char * defKey, int size) {
	if (size == -1 && !StringLengthInt(defKey, &size))
		return -1;

	if (size == 0) {
		_IsDefaultKey = true;
		defKey = defaultString;	//cannot allow empty strings
		size = 8;
	} else {
		_IsDefaultKey = false;
	}
	if ((size_t)size > KeySize)
		size = KeySize;

	memcpy(_DefaultKey, defKey, size);
	for(size_t i = size; i < KeySize; i++) {
		_DefaultKey[i] = 0;	//fill rest with zeroes
	}
	return 0;
}

const char* Encryption::GetDefaultKey() {
	return _DefaultKey;
}

bool Encryption::IsDefaultKey() {
	return _IsDefaultKey;
}

char* Encryption::DES_encrypt(const char * key, int keysize, const char * data, int size, bool addZero, int type) {
	char keybuf[KeySize];
	if (key == NULL) {
		memcpy(keybuf, _DefaultKey, KeySize);
		keysize = KeySize;
	} else {
		if (keysize == -1) {
			// zero terminator NOT included
			if (!StringLengthInt(key, &keysize))
				return NULL;
		}
		if (keysize > KeySize)
			keysize = KeySize;
		memcpy(keybuf, key, keysize);
		for(size_t i = keysize; i < KeySize; i++)
			keybuf[i] = 0;
	}

	if (size == -1 && !StringLengthInt(data, &size))
		return NULL;

	char * decrypted = new char[size+(addZero?1:0)];
	if (!decrypted)
		return NULL;

	if (addZero)
		decrypted[size] = 0;

	// Prepare the key for use with DES_cfb64_encrypt
	DES_cblock key2;
	DES_key_schedule schedule;
	memcpy(key2, keybuf, KeySize);
	DES_set_odd_parity(&key2);
	DES_set_key_checked(&key2, &schedule);

	// Decryption occurs here
	int n = 0;
	DES_cfb64_encrypt((unsigned char*) data, (unsigned char *)decrypted, size, &schedule, &key2, &n, type);

	return decrypted;
}
