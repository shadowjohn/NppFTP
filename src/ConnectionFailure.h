#ifndef CONNECTIONFAILURE_H
#define CONNECTIONFAILURE_H

#include <tchar.h>
#include <libssh/libssh.h>

enum ConnectionFailureKind {
    ConnectionFailureUnknown = 0,
    ConnectionFailureAuthentication
};

// Only a login command rejected with 530 proves an authentication failure.
// 421 (service unavailable), 500 (syntax), TLS and transport errors do not.
inline ConnectionFailureKind ClassifyFtpLoginFailure(bool loginRejected, int reply) {
    return loginRejected && reply == 530 ? ConnectionFailureAuthentication : ConnectionFailureUnknown;
}

// Transport/internal errors and unfinished asynchronous challenges are not a
// credential rejection. Partial authentication means more factors are required.
inline ConnectionFailureKind ClassifySshLoginFailure(int result) {
    return result == SSH_AUTH_DENIED || result == SSH_AUTH_PARTIAL
        ? ConnectionFailureAuthentication : ConnectionFailureUnknown;
}

inline const TCHAR * GetConnectionFailureMessage(ConnectionFailureKind kind) {
    return kind == ConnectionFailureAuthentication
        ? TEXT("Authentication failed. Please check your username, password or login method.")
        : TEXT("Unable to connect. Please check the server and connection settings. See Output for details.");
}

#endif // CONNECTIONFAILURE_H
