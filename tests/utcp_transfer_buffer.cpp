#include "UTCP/include/ut_clnt.h"

#include <stdio.h>

static_assert(WSC_TRANSFER_BUFFER_SIZE == 64 * 1024, "FTP transfer buffer must be 64 KB");

int main()
{
	printf("utcp_transfer_buffer_exit=0\n");
	return 0;
}
