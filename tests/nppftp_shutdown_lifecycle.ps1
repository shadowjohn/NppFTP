param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

function Get-BracedBlock([string]$Text, [string]$Marker) {
    $start = $Text.IndexOf($Marker, [System.StringComparison]::Ordinal)
    if ($start -lt 0) {
        throw "Marker not found: $Marker"
    }

    $open = $Text.IndexOf('{', $start)
    if ($open -lt 0) {
        throw "Opening brace not found: $Marker"
    }

    $depth = 0
    for ($i = $open; $i -lt $Text.Length; ++$i) {
        if ($Text[$i] -eq '{') { ++$depth }
        elseif ($Text[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                return $Text.Substring($start, $i - $start + 1)
            }
        }
    }

    throw "Closing brace not found: $Marker"
}

function Require-Match([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

function Require-Order([string]$Text, [string]$First, [string]$Second, [string]$Message) {
    $firstIndex = $Text.IndexOf($First, [System.StringComparison]::Ordinal)
    $secondIndex = $Text.IndexOf($Second, [System.StringComparison]::Ordinal)
    if ($firstIndex -lt 0 -or $secondIndex -lt 0 -or $firstIndex -ge $secondIndex) {
        throw $Message
    }
}

$nppSource = Get-Content -LiteralPath (Join-Path $Root 'src\NppFTP.cpp') -Raw
$sessionSource = Get-Content -LiteralPath (Join-Path $Root 'src\FTPSession.cpp') -Raw
$stop = Get-BracedBlock $nppSource 'int NppFTP::Stop()'
$terminate = Get-BracedBlock $sessionSource 'int FTPSession::TerminateSession('

Require-Match $stop 'm_ftpSession->TerminateSession\(false\)' 'Plugin shutdown must stop sessions without leaving background queues alive.'
Require-Match $stop 'm_ftpSession->Deinit\(\)' 'Plugin shutdown must detach the session after workers are stopped.'
Require-Match $stop 'delete m_ftpSession;' 'Plugin shutdown must release the stopped session.'
Require-Match $stop 'm_ftpWindow->Destroy\(\)' 'Plugin shutdown must destroy the FTP window.'
Require-Match $stop 'delete m_ftpWindow;' 'Plugin shutdown must release the FTP window.'
Require-Order $stop 'm_ftpSession->TerminateSession(false);' 'm_ftpWindow->Destroy();' 'Session queues must stop before their notification window is destroyed.'
Require-Order $stop 'm_ftpSession->Deinit();' 'delete m_ftpSession;' 'Plugin shutdown must deinitialize before releasing the session.'
Require-Order $stop 'm_ftpWindow->Destroy();' 'delete m_ftpWindow;' 'Plugin shutdown must destroy before releasing the window.'
Require-Match $terminate 'if \(confirmTransfers && transferCount > 0\)' 'Explicit plugin shutdown must bypass the interactive transfer-confirmation prompt.'

Write-Output 'nppftp_shutdown_lifecycle_exit=0'
