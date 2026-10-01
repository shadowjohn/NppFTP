$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Read-Source([string]$path) { Get-Content -LiteralPath (Join-Path $root $path) -Raw }
$ssh = Read-Source 'src/FTPClientWrapperSSH.cpp'
$ssl = Read-Source 'src/FTPClientWrapperSSL.cpp'
$ui = Read-Source 'src/Windows/FTPWindow.cpp'
$queue = Read-Source 'src/QueueOperation.cpp'
if ($ssh -notmatch '(?s)int FTPClientWrapperSSH::Connect\(\)\s*\{\s*m_connectionFailure = ConnectionFailureUnknown;' -or
    $ssl -notmatch '(?s)int FTPClientWrapperSSL::Connect\(\)\s*\{\s*m_connectionFailure = ConnectionFailureUnknown;') {
    throw 'Connect retries must reset authentication failure state'
}
$auth = $ssh.Substring($ssh.IndexOf('int FTPClientWrapperSSH::authenticate('))
$auth = $auth.Substring(0, $auth.IndexOf('int FTPClientWrapperSSH::authenticate_key('))
if ($auth -notmatch '(?s)if \(methods == 0\) \{\s*m_connectionFailure = ConnectionFailureAuthentication;' -or
    $auth -notmatch 'm_connectionFailure = ClassifySshLoginFailure\(authres\);') {
    throw 'SSH rejection and incompatible authentication methods must be classified'
}
if ($auth -match '(?s)if \(authres == SSH_AUTH_ERROR\) \{[^}]*m_connectionFailure = ConnectionFailureAuthentication') {
    throw 'SSH transport/internal errors must not be credential rejection'
}
$start = $ui.IndexOf('case QueueOperation::QueueTypeConnect: {', $ui.IndexOf('int FTPWindow::OnEvent('))
$connect = $ui.Substring($start, $ui.IndexOf('case QueueOperation::QueueTypeDisconnect:', $start) - $start)
if (($connect | Select-String -Pattern '::MessageBox\(' -AllMatches).Matches.Count -ne 1 -or
    $connect.IndexOf('GetConnectionFailureKind()') -gt $connect.IndexOf('TerminateSession()') -or
    $connect.IndexOf('::MessageBox(') -lt $connect.IndexOf('TerminateSession()')) {
    throw 'Exactly one connect prompt must use a snapshot and follow session cleanup'
}
if ($queue -notmatch '(?s)m_result = m_client->Connect\(\);\s*m_connectionFailure = m_client->GetConnectionFailureKind\(\);' -or
    $queue -notmatch 'do not send duplicate notifications' -or $queue -notmatch '::PostMessage\(m_hNotify, msg') {
    throw 'Queue completion must snapshot classification and deliver through existing UI notification path'
}
Write-Output 'connection_failure_source_contract: retry reset, SSH branches, one UI-thread prompt and queue snapshot passed'
