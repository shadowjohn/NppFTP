#include "UTCP/include/ftp_pasv.h"

#include <assert.h>
#include <string.h>

int main()
{
	char ip[20];
	int port = 0;

	assert(CUT_ParsePASVResponse("227 Entering Passive Mode (192,168,1,2,12,34)", ip, sizeof(ip), &port));
	assert(strcmp(ip, "192.168.1.2") == 0);
	assert(port == 3106);

	assert(CUT_ParsePASVEndpoint("227 Entering Passive Mode (10,0,0,5,195,80)", "203.0.113.9", ip, sizeof(ip), &port));
	assert(strcmp(ip, "203.0.113.9") == 0);
	assert(port == 50000);
	assert(!CUT_ParsePASVEndpoint("227 Entering Passive Mode (10,0,0,5,195,80)", "", ip, sizeof(ip), &port));

	assert(!CUT_ParsePASVResponse("227 Entering Passive Mode (256,168,1,2,12,34)", ip, sizeof(ip), &port));
	assert(!CUT_ParsePASVResponse("227 Entering Passive Mode (1,2,3,4,999999999999999999,1)", ip, sizeof(ip), &port));
	assert(!CUT_ParsePASVResponse("227 Entering Passive Mode (1,2,3,4,1)", ip, sizeof(ip), &port));
	assert(!CUT_ParsePASVResponse("227 Entering Passive Mode (1,2,3,4,0,0)", ip, sizeof(ip), &port));
	assert(!CUT_ParsePASVResponse("227 Entering Passive Mode (1,2,three,4,1,1)", ip, sizeof(ip), &port));

	return 0;
}
