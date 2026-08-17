param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

function Read-Source([string]$Path) {
    return [System.IO.File]::ReadAllText((Join-Path $Root $Path))
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

$sessionHeader = Read-Source 'src/FTPSession.h'
$sessionSource = Read-Source 'src/FTPSession.cpp'
$queueHeader = Read-Source 'src/QueueOperation.h'
$queueSource = Read-Source 'src/FTPQueue.cpp'
$windowSource = Read-Source 'src/Windows/FTPWindow.cpp'

Require-Match $sessionHeader 'ConcurrentUploadScheduler\s*\*\s*m_uploadScheduler' 'FTPSession must own the upload scheduler.'
Require-Match $sessionHeader 'UploadPriority\s+priority\s*=\s*UploadPriorityNormal' 'UploadFile must default to normal priority.'
Require-Match $sessionSource 'new ConcurrentUploadScheduler\(m_hNotify,\s*m_mainWrapper,\s*m_ftpSettings->GetMaxConcurrentUploads\(\)\)' 'StartSession must construct the scheduler from the configured main wrapper and global worker count.'
Require-Order $sessionSource 'm_mainWrapper->SetCertificates(m_certificates);' 'new ConcurrentUploadScheduler' 'Worker clones must be created only after the main wrapper is fully configured.'
Require-Match $sessionSource 'if \(m_uploadScheduler->Initialize\(\) != 0\)\s*\{\s*Clear\(\);' 'Scheduler initialization failure must use the ordered session cleanup path.'
Require-Match $sessionSource 'm_uploadScheduler->AddQueueOp\(uldop,\s*priority\)' 'Manual UploadFile operations must route through the upload scheduler.'
Require-Match $sessionSource 'm_transferQueue->AddQueueOp\(dldop\)' 'Downloads must remain on the serial transfer queue.'
Require-Match $sessionSource 'm_transferQueue->AddQueueOp\(new QueueRemoteDownloadComplete' 'Recursive downloads must remain serial.'
Require-Match $sessionSource 'm_transferQueue->AddQueueOp\(complete\)' 'Recursive upload restructuring belongs to Task 4 and must remain serial here.'
Require-Match $sessionSource 'UploadFile\(sourcefile,\s*target,\s*false,\s*0\)' 'Automatic saves must still use the default normal priority until Task 5.'
Require-Match $sessionSource 'm_uploadScheduler->AbortActive\(\)' 'AbortTransfer must abort active upload workers.'
Require-Match $sessionSource 'cancelOp->GetType\(\)\s*==\s*QueueOperation::QueueTypeUpload' 'Upload cancellation must consult the upload scheduler.'
Require-Match $sessionSource 'transferCount \+= m_uploadScheduler->GetQueueSize\(\)' 'Disconnect confirmation must include scheduled uploads.'
Require-Order $sessionSource 'delete m_uploadScheduler;' 'delete m_transferWrapper;' 'Clear must destroy the upload scheduler before its prototype wrapper.'
Require-Match $queueHeader 'virtual\s+QueueOperation\s*\*\s*OnQueueTerminal\(\)' 'QueueOperation must expose the terminal follow-up hook.'
Require-Order $queueSource 'SendNotification(QueueOperation::QueueEventEnd)' 'OnQueueTerminal()' 'The terminal hook must run only after the End notification is acknowledged.'
Require-Match $windowSource 'm_activeTransferCount' 'FTPWindow must count concurrent active transfers instead of clearing busy state on the first End event.'

Write-Output 'ftp_session_upload_routing_exit=0'
