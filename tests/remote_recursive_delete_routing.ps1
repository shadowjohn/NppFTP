$ErrorActionPreference = 'Stop'

function Require-Contains {
    param([string] $Content, [string] $Needle, [string] $Message)

    if (-not $Content.Contains($Needle)) {
        throw "$Message Missing: $Needle"
    }
}

$sessionHeader = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\FTPSession.h')
$sessionSource = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\FTPSession.cpp')
$windowSource = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\Windows\FTPWindow.cpp')

Require-Contains $sessionHeader 'RmDirRecursive(const char * path)' 'Session API must expose recursive directory deletion.'
Require-Contains $sessionSource 'QueueRemoveDirRecursive * dirop = new QueueRemoveDirRecursive' 'Session must enqueue the recursive operation.'
Require-Contains $windowSource 'm_ftpSession->RmDirRecursive(dir->GetPath())' 'Remote Browser delete must use recursive deletion.'
Require-Contains $windowSource 'Delete this directory and all files and subdirectories inside it?' 'Confirmation must disclose recursive deletion.'
Require-Contains $windowSource '[FTPWindow] Unable to remove directory tree %T at %T' 'Failure output must include the exact failed path.'
Require-Contains $windowSource '[FTPWindow] Refusing to remove the remote root directory' 'Remote root deletion must be blocked by the UI.'

Write-Output 'remote_recursive_delete_routing_exit=0'
