// Focused command:
// cmake --build _build --config Release --target NppFTP_FTPSettingsRoundTrip

#include "ConcurrentUploadSettings.h"
#include "tinyxml.h"

#include <stdio.h>

static bool CheckValue(const char * name, int actual, int expected)
{
	if (actual == expected)
		return true;
	fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
	return false;
}

int main()
{
	TiXmlElement elementWithoutValue("Settings");
	TiXmlElement element("Settings");

	if (!CheckValue("Normalize(0)", ConcurrentUploadSettings::Normalize(0), 1)) return 1;
	if (!CheckValue("Normalize(-1)", ConcurrentUploadSettings::Normalize(-1), 1)) return 1;
	if (!CheckValue("Normalize(1)", ConcurrentUploadSettings::Normalize(1), 1)) return 1;
	if (!CheckValue("Normalize(8)", ConcurrentUploadSettings::Normalize(8), 8)) return 1;
	if (!CheckValue("Normalize(9)", ConcurrentUploadSettings::Normalize(9), 1)) return 1;
	if (!CheckValue("Load(missing)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	elementWithoutValue.SetAttribute("maxConcurrentUploads", 0);
	if (!CheckValue("Load(0)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	elementWithoutValue.SetAttribute("maxConcurrentUploads", -1);
	if (!CheckValue("Load(-1)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	elementWithoutValue.SetAttribute("maxConcurrentUploads", 9);
	if (!CheckValue("Load(9)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	elementWithoutValue.SetAttribute("maxConcurrentUploads", "abc");
	if (!CheckValue("Load(abc)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	elementWithoutValue.SetAttribute("maxConcurrentUploads", "2.5");
	if (!CheckValue("Load(2.5)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	elementWithoutValue.SetAttribute("maxConcurrentUploads", "2abc");
	if (!CheckValue("Load(2abc)", ConcurrentUploadSettings::Load(&elementWithoutValue), 1)) return 1;
	ConcurrentUploadSettings::Save(&element, 8);
	if (!CheckValue("Load(saved 8)", ConcurrentUploadSettings::Load(&element), 8)) return 1;

	printf("ftp_settings_roundtrip_exit=0\n");
	return 0;
}
