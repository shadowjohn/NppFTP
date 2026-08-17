// Focused command:
// cmake --build _build --config Release --target NppFTP_FTPSettingsRoundTrip

#include "ConcurrentUploadSettings.h"
#include "tinyxml.h"

#include <assert.h>
#include <stdio.h>

int main()
{
	TiXmlElement elementWithoutValue("Settings");
	TiXmlElement element("Settings");

	assert(ConcurrentUploadSettings::Normalize(0) == 1);
	assert(ConcurrentUploadSettings::Normalize(-1) == 1);
	assert(ConcurrentUploadSettings::Normalize(1) == 1);
	assert(ConcurrentUploadSettings::Normalize(8) == 8);
	assert(ConcurrentUploadSettings::Normalize(9) == 1);
	assert(ConcurrentUploadSettings::Load(&elementWithoutValue) == 1);
	elementWithoutValue.SetAttribute("maxConcurrentUploads", 0);
	assert(ConcurrentUploadSettings::Load(&elementWithoutValue) == 1);
	elementWithoutValue.SetAttribute("maxConcurrentUploads", -1);
	assert(ConcurrentUploadSettings::Load(&elementWithoutValue) == 1);
	elementWithoutValue.SetAttribute("maxConcurrentUploads", 9);
	assert(ConcurrentUploadSettings::Load(&elementWithoutValue) == 1);
	ConcurrentUploadSettings::Save(&element, 8);
	assert(ConcurrentUploadSettings::Load(&element) == 8);

	printf("ftp_settings_roundtrip_exit=0\n");
	return 0;
}
