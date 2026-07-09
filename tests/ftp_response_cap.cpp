#include "UTCP/include/stdafx.h"
#include "UTCP/include/ftp_c.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <process.h>

static unsigned short g_port = 0;
static HANDLE g_server_ready_event = NULL;

unsigned __stdcall MockFtpServer(void* pParam) {
	SOCKET listen_sock = (SOCKET)pParam;

	// Accept the connection
	printf("Server thread: accept waiting...\n");
	fflush(stdout);
	SOCKET client_sock = accept(listen_sock, NULL, NULL);
	if (client_sock == INVALID_SOCKET) {
		printf("Server thread: accept failed, WSAGetLastError = %d\n", WSAGetLastError());
		fflush(stdout);
		return 0;
	}
	printf("Server thread: accept succeeded!\n");
	fflush(stdout);

	// 1. Send greeting multiline response: 220- followed by 10,005 lines
	// then the end line "220 Ready" (which shouldn't be reached if capped)
	int sent = send(client_sock, "220-Welcome to mock FTP\r\n", 25, 0);
	if (sent == SOCKET_ERROR) {
		printf("Server thread: send failed, WSAGetLastError = %d\n", WSAGetLastError());
		fflush(stdout);
	}

	char buf[64];
	for (int i = 0; i < 10005; ++i) {
		sprintf(buf, "220-Line %d\r\n", i);
		send(client_sock, buf, (int)strlen(buf), 0);
	}
	send(client_sock, "220 Ready\r\n", 11, 0);

	// Let the client read and close
	Sleep(1000);
	closesocket(client_sock);
	return 0;
}

int main()
{
	// Initialize Winsock
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	assert(result == 0);

	// Create listening socket
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert(listen_sock != INVALID_SOCKET);

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = 0; // bind to dynamic port

	result = bind(listen_sock, (SOCKADDR*)&service, sizeof(service));
	assert(result == 0);

	// Get port
	int name_len = sizeof(service);
	result = getsockname(listen_sock, (SOCKADDR*)&service, &name_len);
	assert(result == 0);
	g_port = ntohs(service.sin_port);

	result = listen(listen_sock, 1);
	assert(result == 0);

	// Start server thread
	HANDLE thread_handle = (HANDLE)_beginthreadex(NULL, 0, MockFtpServer, (void*)listen_sock, 0, NULL);
	assert(thread_handle != NULL);

	// Client connection
	CUT_FTPClient client;
	client.SetConnectTimeout(2);
	client.SetReceiveTimeOut(2000);

	int conn_res = client.Connect(g_port, "127.0.0.1");

	printf("Connect result: %d\n", conn_res);

	// Check the multiline response line count
	LONG count = client.GetMultiLineResponseLineCount();
	printf("Multiline line count: %ld\n", count);
	fflush(stdout);

	// Clean up
	closesocket(listen_sock);
	WaitForSingleObject(thread_handle, INFINITE);
	CloseHandle(thread_handle);
	WSACleanup();

	assert(conn_res != 0);
	assert(count == 10000);

	printf("ftp_response_cap_exit=0\n");
	return 0;
}
