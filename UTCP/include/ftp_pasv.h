#ifndef __CUT_FTP_PASV_HELPERS
#define __CUT_FTP_PASV_HELPERS

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool CUT_ParsePASVByte(const char **cursor, char separator, int *value)
{
	const char *p = *cursor;
	unsigned int n = 0;

	if (!p || !isdigit((unsigned char)*p))
		return false;

	while (isdigit((unsigned char)*p)) {
		n = (n * 10) + (unsigned int)(*p - '0');
		if (n > 255)
			return false;
		p++;
	}

	if (*p != separator)
		return false;

	*value = (int)n;
	*cursor = p + 1;
	return true;
}

static bool CUT_ParsePASVResponse(const char *response, char *ipAddress, size_t ipAddressSize, int *port)
{
	int parts[6] = {};
	const char *p = response ? strchr(response, '(') : NULL;

	if (!p || !ipAddress || ipAddressSize < 16 || !port)
		return false;

	p++;
	for (int i = 0; i < 6; i++) {
		if (!CUT_ParsePASVByte(&p, i == 5 ? ')' : ',', &parts[i]))
			return false;
	}

	*port = (parts[4] * 256) + parts[5];
	if (*port <= 0)
		return false;

	sprintf(ipAddress, "%d.%d.%d.%d", parts[0], parts[1], parts[2], parts[3]);
	return true;
}

#endif
