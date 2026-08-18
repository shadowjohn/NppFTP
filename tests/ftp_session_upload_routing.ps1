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
$schedulerHeader = Read-Source 'src/ConcurrentUploadScheduler.h'
$schedulerSource = Read-Source 'src/ConcurrentUploadScheduler.cpp'
$ftpQueueHeader = Read-Source 'src/FTPQueue.h'
$queueHeader = Read-Source 'src/QueueOperation.h'
$queueSource = Read-Source 'src/FTPQueue.cpp'
$terminalLifecycle = Read-Source 'src/QueueTerminalLifecycle.h'
$uploadPlanSource = Read-Source 'src/RemoteUploadPlan.cpp'
$operationSource = Read-Source 'src/QueueOperation.cpp'
$certificateLockSource = Read-Source 'src/CertificateStoreLock.cpp'
$sslSource = Read-Source 'src/FTPClientWrapperSSL.cpp'
$certificateSource = Read-Source 'src/SSLCertificates.cpp'
$windowSource = Read-Source 'src/Windows/FTPWindow.cpp'

Require-Match $sessionHeader 'ConcurrentUploadScheduler\s*\*\s*m_uploadScheduler' 'FTPSession must own the upload scheduler.'
Require-Match $sessionHeader 'UploadPriority\s+priority\s*=\s*UploadPriorityNormal' 'UploadFile must default to normal priority.'
Require-Match $sessionSource 'new ConcurrentUploadScheduler\(m_hNotify,\s*m_mainWrapper,\s*m_ftpSettings->GetMaxConcurrentUploads\(\),\s*m_transferQueue\)' 'StartSession must construct the scheduler with configured workers and the serial terminal sink.'
Require-Order $sessionSource 'm_mainWrapper->SetCertificates(m_certificates);' 'new ConcurrentUploadScheduler' 'Worker clones must be created only after the main wrapper is fully configured.'
Require-Match $sessionSource 'if \(m_uploadScheduler->Initialize\(\) != 0\)\s*\{\s*Clear\(\);' 'Scheduler initialization failure must use the ordered session cleanup path.'
Require-Match $sessionSource 'm_uploadScheduler->AddQueueOp\(uldop,\s*priority\)' 'Manual UploadFile operations must route through the upload scheduler.'
Require-Match $sessionSource 'm_transferQueue->AddQueueOp\(dldop\)' 'Downloads must remain on the serial transfer queue.'
Require-Match $sessionSource 'm_transferQueue->AddQueueOp\(new QueueRemoteDownloadComplete' 'Recursive downloads must remain serial.'
Require-Match $sessionSource 'm_transferQueue->AddQueueOp\(prepare\)' 'Recursive directory preparation must remain on the serial transfer queue.'
Require-Match $sessionSource 'm_uploadScheduler->AddQueueOp\(upload,\s*UploadPriorityNormal\)' 'Selected recursive files must route through normal-priority upload workers.'
if ($sessionSource -match 'm_transferQueue->AddQueueOp\(upload\)') {
    throw 'Recursive file uploads must not remain on the serial transfer queue.'
}
Require-Match $windowSource 'QueueTypeRemoteUploadPrepare[\s\S]*?DispatchRemoteUploadBatch\(batch\)' 'Selected files must dispatch only after directory preparation ends.'
Require-Match $windowSource 'QueueTypeRemoteUploadPrepare[\s\S]*?should_dispatch_remote_upload_after_prepare\(queueResult,\s*queueOp->WasCanceled\(\)\)[\s\S]*?break;[\s\S]*?DispatchRemoteUploadBatch\(batch\)' 'Failed or canceled directory preparation must not dispatch selected files.'
Require-Match $sessionSource 'UploadFile\(sourcefile,\s*target,\s*false,\s*0,\s*UploadPriorityUrgent\)' 'Automatic saves must use urgent upload priority.'
Require-Match $sessionSource 'm_uploadScheduler->AbortActive\(\)' 'AbortTransfer must abort active upload workers.'
Require-Match $sessionSource 'm_transferQueue\s*&&\s*m_transferQueue->AbortActive\(\)' 'AbortTransfer must route serial prepare Abort through the active queue state.'
Require-Match $sessionSource 'cancelOp->GetType\(\)\s*==\s*QueueOperation::QueueTypeUpload' 'Upload cancellation must consult the upload scheduler.'
Require-Match $sessionSource 'transferCount \+= m_uploadScheduler->GetQueueSize\(\)' 'Disconnect confirmation must include scheduled uploads.'
Require-Order $sessionSource 'delete m_uploadScheduler;' 'delete m_transferWrapper;' 'Clear must destroy the upload scheduler before its prototype wrapper.'
Require-Match $sessionSource 'm_transferQueue->ClearQueue\(true\)' 'Session teardown must explicitly suppress transfer queue completion summaries.'
Require-Match $sessionSource 'int FTPSession::Clear\(\)[\s\S]*?InterlockedIncrement\(&m_generation\);[\s\S]*?m_transferQueue->BeginTeardown\(\);[\s\S]*?m_uploadScheduler->Deinitialize\(\);' 'Session teardown must invalidate the generation and close the serial queue before waiting for upload workers.'
Require-Order $sessionSource 'm_uploadScheduler->Deinitialize();' 'DiscardRemoteUploadBatchNotifications();' 'Session teardown must drain ref-counted completion payloads after workers stop.'
Require-Match $queueHeader 'virtual\s+QueueOperation\s*\*\s*OnQueueTerminal\(\)' 'QueueOperation must expose the terminal follow-up hook.'
Require-Match $ftpQueueHeader 'ClearQueue\(bool suppressTerminalFollowUps = false\)' 'Queue clearing must distinguish normal cancellation from session teardown.'
Require-Match $queueSource 'queue_end_and_take_terminal\(op,\s*QueueOperation::QueueEventEnd\)' 'The queue must acknowledge End before taking the terminal follow-up.'
Require-Match $terminalLifecycle 'queue_cancel_and_take_terminal' 'Cancellation paths must share the generic terminal lifecycle helper.'
if ([regex]::Matches(($schedulerSource + $queueSource), 'queue_cancel_and_take_terminal').Count -lt 5) {
    throw 'Scheduler rejection, merge, cleanup, and queue teardown must all take terminal follow-ups.'
}
Require-Match $uploadPlanSource 'PostMessage\(m_completionWindow,\s*NotifyMessageRemoteUploadBatchComplete' 'Final batch terminal must post a nonblocking ref-counted UI completion request.'
Require-Match $windowSource 'NotifyMessageRemoteUploadBatchComplete[\s\S]*?HandleRemoteUploadBatchCompletion' 'FTPWindow must hand batch completion requests to the active session on the UI thread.'
Require-Match $sessionSource 'HandleRemoteUploadBatchCompletion[\s\S]*?generation != InterlockedCompareExchange\(&m_generation[\s\S]*?new QueueRemoteUploadComplete' 'Only the active session generation may create the existing completion marker.'
if ($schedulerSource -match 'm_terminalQueue->AddQueueOp\(followUp\)') {
    throw 'Worker threads must not enqueue terminal markers through an acknowledging queue path.'
}
if ($operationSource -match 'return new QueueRemoteUploadComplete') {
    throw 'Worker-owned recursive operations must post batch completion instead of constructing queue markers.'
}
Require-Match $queueSource 'queue_filter_terminal_follow_up\(terminalOp,\s*suppressTerminalFollowUps\)' 'Transfer queue teardown must account canceled batches while suppressing their UI summaries.'
Require-Match $queueSource 'AbortActive[\s\S]*?m_activeOp->CancelExecution\(false,\s*&wasRunning\)[\s\S]*?OnQueueCanceled\(\)[\s\S]*?m_wrapper->Abort\(\)' 'Active Abort must atomically claim an executing operation before marking it canceled.'
Require-Match $operationSource 'CompletePerform[\s\S]*?FinishExecution\(\)' 'A completed operation result must linearize before Perform returns.'
Require-Match $operationSource 'QueueUpload::Perform[\s\S]*?CompletePerform\(m_client->SendFile' 'Uploads must finalize their actual transfer result before late Abort can relabel it.'
Require-Match $schedulerSource 'Deinitialize[\s\S]*?queue->BeginTeardown\(\)' 'Scheduler shutdown must cancel worker operations that are still in the pre-execution idle state.'
Require-Match $queueSource 'if \(!m_teardown\)\s*m_queue\.front\(\)->SendNotification\(QueueOperation::QueueEventEnd\)' 'Confirmed teardown must not synthesize an active End notification on the UI thread.'
Require-Match $operationSource 'QueueRemoteUploadPrepare::OnQueueTeardown[\s\S]*?CancelUnstartedSelectedFilesOnce' 'Undispatched selected files must be terminal-accounted when prepare is torn down after Perform.'
Require-Match $schedulerSource 'int ConcurrentUploadScheduler::AbortActive\(\)[\s\S]*?int result = 0;[\s\S]*?queue->AbortActive\(\) != 0[\s\S]*?result = -1;[\s\S]*?return result;' 'Active Abort must attempt every worker before returning an aggregate result.'
Require-Match $windowSource 'm_activeTransferCount' 'FTPWindow must count concurrent active transfers instead of clearing busy state on the first End event.'
Require-Match ($schedulerHeader + $schedulerSource) 'std::deque<QueueOperation\s*\*>' 'Scheduler workers must retain ownership visibility until terminal removal.'
Require-Match $schedulerSource 'FindWorkerConflictLocked' 'Dispatch must reject same-file work that conflicts with an assigned worker operation.'
Require-Match $operationSource 'upload_transfer_paths_conflict' 'QueueUpload conflict checks must use the executable-tested path identity helper.'
Require-Order $schedulerSource 'worker->operations.push_back(op);' 'worker->queue->AddQueueOp(op, false)' 'Scheduler ownership must be registered before worker queue adoption.'
if ([regex]::Matches($schedulerSource, 'SendNotification\(QueueOperation::QueueEventAdd\)').Count -ne 1) {
    throw 'Scheduler must emit exactly one QueueEventAdd per accepted operation.'
}
Require-Match $ftpQueueHeader 'AddQueueOp\(QueueOperation \* op, bool sendAddNotification' 'Worker adoption must explicitly suppress the second Add notification.'
Require-Match $queueSource 'm_threadHandle\s*=\s*::CreateThread' 'FTPQueue must retain the worker thread handle.'
Require-Match $queueSource 'if \(!m_threadHandle\)' 'FTPQueue must report CreateThread failure.'
Require-Match $queueSource '::WaitForSingleObject\(threadHandle, INFINITE\)' 'FTPQueue must join its worker thread before closing the handle.'
Require-Match $queueSource '::CloseHandle\(threadHandle\)' 'FTPQueue must close its worker thread handle.'
Require-Match $queueSource 'if \(sendAddNotification\)\s*op->SendNotification\(QueueOperation::QueueEventAdd\)' 'FTPQueue must honor scheduler adoption without a second Add notification.'
Require-Order $queueSource 'm_queue.pop_front();' 'm_terminalCallback(m_terminalContext, op, NULL);' 'FTPQueue must release its completed queue slot before the scheduler wakes for another dispatch.'
Require-Match $queueSource 'op->SendNotification\(QueueOperation::QueueEventRemove\);[\s\S]*?m_queue\.pop_front\(\);[\s\S]*?m_monitor->Exit\(\);[\s\S]*?m_terminalCallback\(m_terminalContext, op, NULL\);[\s\S]*?delete op;' 'Completed worker operations must follow Remove, queue release, scheduler terminal callback, then deletion.'
Require-Match $certificateLockSource 'static CertificateStoreLockState \* state = new CertificateStoreLockState\(\)' 'The certificate lock must initialize once and remain valid through global teardown.'
if ($certificateLockSource -match 'DeleteCriticalSection') {
    throw 'The certificate lock must remain valid through global NppFTP teardown.'
}
if ([regex]::Matches($sslSource, 'CertificateStoreLock\s+lock').Count -lt 2 -or
    [regex]::Matches($certificateSource, 'CertificateStoreLock\s+lock').Count -lt 3) {
    throw 'Every shared FTPS certificate read/write path must hold the certificate store lock.'
}

Write-Output 'ftp_session_upload_routing_exit=0'
