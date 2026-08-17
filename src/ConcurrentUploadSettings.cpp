/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#include "ConcurrentUploadSettings.h"

#include "tinyxml.h"

#include <limits.h>

namespace {
	const char * const kMaxConcurrentUploadsAttribute = "maxConcurrentUploads";
	const int kMinConcurrentUploads = 1;
	const int kMaxConcurrentUploads = 8;

	bool ParseInteger(const char * text, int * value) {
		if (!text || !*text)
			return false;

		bool negative = false;
		if (*text == '-' || *text == '+') {
			negative = (*text == '-');
			++text;
		}
		if (!*text)
			return false;

		int parsed = 0;
		for (; *text; ++text) {
			if (*text < '0' || *text > '9')
				return false;
			int digit = *text - '0';
			if (parsed > (INT_MAX - digit) / 10)
				return false;
			parsed = parsed * 10 + digit;
		}

		*value = negative ? -parsed : parsed;
		return true;
	}
}

int ConcurrentUploadSettings::Normalize(int value) {
	if (value < kMinConcurrentUploads || value > kMaxConcurrentUploads)
		return kMinConcurrentUploads;
	return value;
}

int ConcurrentUploadSettings::Load(const TiXmlElement * settingsElem) {
	int value = 0;
	if (!settingsElem || !ParseInteger(settingsElem->Attribute(kMaxConcurrentUploadsAttribute), &value))
		return kMinConcurrentUploads;
	return Normalize(value);
}

void ConcurrentUploadSettings::Save(TiXmlElement * settingsElem, int value) {
	if (settingsElem)
		settingsElem->SetAttribute(kMaxConcurrentUploadsAttribute, Normalize(value));
}
