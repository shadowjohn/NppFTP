/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#ifndef UPLOADTRANSFERIDENTITY_H
#define UPLOADTRANSFERIDENTITY_H

#include <string.h>
#include <tchar.h>

inline bool upload_transfer_paths_conflict(const TCHAR * localA, const char * remoteA,
	const TCHAR * localB, const char * remoteB) {
	return localA && remoteA && localB && remoteB &&
		_tcscmp(localA, localB) == 0 && strcmp(remoteA, remoteB) == 0;
}

#endif //UPLOADTRANSFERIDENTITY_H
