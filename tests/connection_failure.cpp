#include "src/StdInc.h"
#include "src/FTPClientWrapper.h"
#include <stdio.h>
#include <thread>

// No logs, credentials, certificate store or remote endpoints are used by this test.
Output * _MainOutput = NULL;
HWND _MainOutputWindow = NULL;
int OutputDebug(const TCHAR *, ...) { return 0; }
int OutputMsg(const TCHAR *, ...) { return 0; }
int OutputClnt(const TCHAR *, ...) { return 0; }
int OutputErr(const TCHAR *, ...) { return 0; }
int MessageBoxOutput(const TCHAR *) { return 0; }

struct Reply { int user; int password; int account; };

static bool Check(bool ok, const char * description) {
    if (!ok) fprintf(stderr, "FAIL: %s\n", description);
    return ok;
}

static bool SendReply(SOCKET peer, int code) {
    if (code == -1) { Sleep(1500); return false; } // response timeout
    if (!code) return false; // simulate server closing without a response
    char reply[64];
    sprintf_s(reply, "%d Local test reply\r\n", code);
    return send(peer, reply, (int)strlen(reply), 0) != SOCKET_ERROR;
}

static void Serve(SOCKET listener, const Reply * replies, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        SOCKET peer = accept(listener, NULL, NULL);
        if (peer == INVALID_SOCKET) return;
        DWORD timeout = 3000;
        setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
        SendReply(peer, 220);
        std::string command;
        char ch;
        while (recv(peer, &ch, 1, 0) == 1) {
            command += ch;
            if (ch != '\n') continue;
            int code = 200;
            bool quit = false;
            if (command.find("USER ") == 0) code = replies[i].user;
            else if (command.find("PASS ") == 0) code = replies[i].password;
            else if (command.find("ACCT ") == 0) code = replies[i].account;
            else if (command.find("QUIT") == 0) { code = 221; quit = true; }
            command.clear();
            if (!SendReply(peer, code) || quit) break;
        }
        closesocket(peer);
    }
}

int main() {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (sockaddr *)&address, sizeof(address)) || listen(listener, 1)) return 1;
    int length = sizeof(address);
    getsockname(listener, (sockaddr *)&address, &length);
    const Reply replies[] = {
        {331, 530, 0}, // wrong password
        {331, 230, 0}, // same client succeeds on retry
        {530, 0, 0},   // username denied
        {331, 421, 0}, // service unavailable during password exchange
        {331, 500, 0}, // protocol error during password exchange
        {331, 0, 0},   // connection lost while authenticating
        {331, -1, 0},  // no response before the receive timeout
        {331, 332, 530}, // additional account rejected
        {230, 0, 0}    // user accepted without password
    };
    const ConnectionFailureKind expected[] = {
        ConnectionFailureAuthentication, ConnectionFailureUnknown,
        ConnectionFailureAuthentication, ConnectionFailureUnknown,
        ConnectionFailureUnknown, ConnectionFailureUnknown,
        ConnectionFailureUnknown,
        ConnectionFailureAuthentication, ConnectionFailureUnknown
    };
    std::thread server(Serve, listener, replies, _countof(replies));
    bool ok = true;
    {
        FTPClientWrapperSSL client("127.0.0.1", ntohs(address.sin_port), "mock-user", "mock-password");
        client.SetTimeout(1);
        for (size_t i = 0; i < _countof(replies); ++i) {
            int result = client.Connect();
            bool success = i == 1 || i == 8;
            ok &= Check(result == (success ? 0 : -1), "connect result");
            ok &= Check(client.GetConnectionFailureKind() == expected[i], "failure classification and reset");
            if (success) client.Disconnect();
        }
    }
    server.join();
    closesocket(listener);
    // The now-closed local port verifies TCP refusal separately from login rejection.
    {
        FTPClientWrapperSSL client("127.0.0.1", ntohs(address.sin_port), "mock-user", "mock-password");
        client.SetTimeout(1);
        ok &= Check(client.Connect() == -1 && client.GetConnectionFailureKind() == ConnectionFailureUnknown,
            "TCP refusal is not password failure");
    }
    ok &= Check(ClassifyFtpLoginFailure(false, 530) == ConnectionFailureUnknown,
        "530 outside login is not authentication failure");
    ok &= Check(_tcscmp(GetConnectionFailureMessage(ConnectionFailureAuthentication),
        TEXT("Authentication failed. Please check your username, password or login method.")) == 0,
        "safe ambiguous authentication message");
    const int sshResults[] = {SSH_AUTH_DENIED, SSH_AUTH_PARTIAL, SSH_AUTH_ERROR,
        SSH_AUTH_AGAIN, SSH_AUTH_INFO, SSH_AUTH_SUCCESS};
    for (size_t i = 0; i < _countof(sshResults); ++i)
        ok &= Check(ClassifySshLoginFailure(sshResults[i]) ==
            (i < 2 ? ConnectionFailureAuthentication : ConnectionFailureUnknown),
            "libssh result policy distinguishes rejection from transport/internal errors");
    WSACleanup();
    if (ok) puts("connection_failure: 9 loopback scenarios, TCP refusal and message policy passed");
    return ok ? 0 : 1;
}
