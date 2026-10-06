# Authentication failure prompt validation (2026-10-01)

The initial profile connection now displays one owned error dialog when it fails.
Authentication rejection says: "Authentication failed. Please check your username,
password or login method." It does not claim the password alone is wrong.
Other connection failures say: "Unable to connect. Please check the server and
connection settings. See Output for details." These follow the existing English
FTP window dialogs; the repository has no general UI language-switching framework.

## Cause and implementation

FTP/FTPS/FTPES use CUT_FTPClient::FTPConnect, which distinguishes rejected USER,
PASS and ACCT commands from network and TLS failures. FTPClientWrapperSSL had
reduced these results to -1. It now retains an authentication classification only
when one of these login commands is rejected with FTP reply 530. Replies 421,
500, missing replies, TCP errors and TLS errors retain the generic classification.
The same path serves plain FTP, explicit FTPES and implicit FTPS.

SFTP connects and verifies the host key before authenticate(). Its authentication
result is classified only for SSH_AUTH_DENIED or SSH_AUTH_PARTIAL, or when no
server authentication method matches the configured methods. SSH_AUTH_ERROR,
SSH_AUTH_AGAIN, SSH_AUTH_INFO, transport/host-key failures and SFTP channel
initialization failures stay generic. Password, public-key and keyboard-interactive
rejection all receive the ambiguous authentication wording.

Each Connect resets the classification. QueueConnect snapshots it after Perform;
the return value remains 0/-1, preserving existing queue and worker contracts.
FTPWindow::OnEvent handles the existing NotifyMessageEnd on the UI thread, copies
the snapshot before TerminateSession destroys the queue, and opens the dialog after
cleanup. QueueOperation already suppresses repeated terminal notifications.
There is one prompt per initial profile connection attempt. Auxiliary upload/data
connections use their existing transfer error behavior and do not generate this
new connection dialog. No additional authentication attempts or logging were added.

No AGENTS.md was found at the repository, D:/mytools or D:/ parent; repository
.agents and .codex directories were absent. README.md, BUILDING.md, CMakeLists.txt,
build_scripts.ps1 and existing tests were inspected. No credentials, private keys,
profile settings or persistent access configuration were accessed.

## Completed checks

- Windows x64 Release plugin and both affected test targets compile successfully.
  Existing legacy encoding/conversion warnings remain. BuildProjectReferences=false
  uses the existing third-party libraries and avoids running download prerequisites.
- NppFTP_ConnectionFailure: nine loopback scenarios use the real SSL wrapper and
  UTCP FTP client: wrong-password 530, success on the same client after failure,
  USER 530, PASS 421, PASS 500, peer disconnect, receive timeout, ACCT 530 and
  passwordless USER 230. TCP refusal on the closed loopback port remains generic.
  All pass, exit 0. The same executable checks libssh DENIED/PARTIAL/ERROR/AGAIN/
  INFO/SUCCESS classifications and the safe authentication wording.
- NppFTP_FTPQueueTerminalLifecycle: existing suite plus mocked initial connection
  rejection, generic failure on retry, snapshot stability after wrapper reuse and
  successful retry. Pass, exit 0.
- tests/connection_failure_source_contract.ps1: reset paths, SFTP branches, snapshot
  before teardown, one UI-thread prompt and existing notification mechanism. Pass.
- Manual diff review: fixed messages contain no secrets or raw server responses;
  error classification is restricted to authentication evidence, success stays silent,
  and no backend popup or automatic retry was introduced.
- git -c core.whitespace=cr-at-eol diff --check: pass (existing files mix LF/CRLF).

## Reproduction

Use the existing configured _build tree and Visual Studio CMake. This environment
exposes PATH and Path simultaneously; launch CMake with a Python subprocess environment
constructed as {k.upper(): v for k, v in os.environ.items()} to avoid MSBuild MSB6001.
Build with:

```
cmake --build _build --config Release --target NppFTP NppFTP_ConnectionFailure NppFTP_FTPQueueTerminalLifecycle -- /p:BuildProjectReferences=false
_build/tests/Release/NppFTP_ConnectionFailure.exe
_build/tests/Release/NppFTP_FTPQueueTerminalLifecycle.exe
powershell -File tests/connection_failure_source_contract.ps1
```

Build log: _build/tests/auth-final-build.log.
Artifact: _build/Release/NppFTP.dll.
SHA-256: 693c39e28e73ffa94fc936105ff587410ea3b43daab86633ddfe8ec33f46d3a5

## Verification limits and remaining acceptance

The user subsequently reported successful NppFTP acceptance and authorized commit
and push. The agent did not perform an independent Notepad++ visual test. Dialog
thread placement and count were checked by source review/contracts.
FTPS/FTPES TLS and a live SFTP authentication exchange were not exercised; their
coverage is shared-path review and libssh-result mock/policy testing, not server
integration. SFTP host-key rejection and SSH transport errors stay upstream of the
new authentication classification by inspection. Existing interactive/host-key dialogs
are outside this change.

For final visual acceptance in an isolated disposable Notepad++ instance, use local
FTP/FTPS/SFTP test servers and disposable accounts: reject login, dismiss one prompt,
retry with a valid login, and verify successful browsing; separately refuse TCP,
expire timeout and reject certificate/host-key trust to verify generic wording.
Do not use production accounts for wrong-password attempts. The initial repair did
not install, commit, push, deploy, package or release anything. Following successful
user acceptance, only the source, tests and this report are submitted for commit and
push; generated binaries remain excluded. No release, tag or installation is included.
