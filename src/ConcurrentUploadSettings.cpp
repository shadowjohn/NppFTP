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

namespace {
	const char * const kMaxConcurrentUploadsAttribute = "maxConcurrentUploads";
	const int kMinConcurrentUploads = 1;
	const int kMaxConcurrentUploads = 8;
}

int ConcurrentUploadSettings::Normalize(int value) {
	if (value < kMinConcurrentUploads || value > kMaxConcurrentUploads)
		return kMinConcurrentUploads;
	return value;
}

int ConcurrentUploadSettings::Load(const TiXmlElement * settingsElem) {
	int value = kMinConcurrentUploads;
	if (!settingsElem || settingsElem->QueryIntAttribute(kMaxConcurrentUploadsAttribute, &value) != TIXML_SUCCESS)
		return kMinConcurrentUploads;
	return Normalize(value);
}

void ConcurrentUploadSettings::Save(TiXmlElement * settingsElem, int value) {
	if (settingsElem)
		settingsElem->SetAttribute(kMaxConcurrentUploadsAttribute, Normalize(value));
}
