// Focused command:
// cl /nologo /EHsc /I src tests\concurrent_upload_scheduler.cpp /Fe:_build\tests\concurrent_upload_scheduler.exe

#include "UploadSchedulingPolicy.h"
#include "UploadTransferIdentity.h"
#include "RemoteUploadPlan.h"
#include "QueueTerminalLifecycle.h"

#include <stdint.h>
#include <stdio.h>

static QueueOperation * FakeOperation(unsigned int value)
{
	return reinterpret_cast<QueueOperation *>(static_cast<uintptr_t>(value));
}

static bool Check(bool condition, const char * message)
{
	if (condition)
		return true;
	fprintf(stderr, "concurrent_upload_scheduler_failed=%s\n", message);
	return false;
}

class BatchFileOperationHarness {
public:
	BatchFileOperationHarness(RemoteUploadBatch * batch, const char * path) :
		m_terminal(batch, path),
		m_endAcknowledged(false),
		m_terminalBeforeEnd(false) {
	}

	void OnQueueCanceled() {
		m_terminal.Cancel();
	}

	bool OnQueueTerminal() {
		if (!m_endAcknowledged)
			m_terminalBeforeEnd = true;
		return m_terminal.Complete();
	}

	int SendNotification(int event) {
		if (event == 2)
			m_endAcknowledged = true;
		return 0;
	}

	bool WasCanceled() const {
		return m_terminal.WasCanceled();
	}

	bool TerminalRanBeforeEnd() const {
		return m_terminalBeforeEnd;
	}

private:
	RemoteUploadFileTerminalState m_terminal;
	bool m_endAcknowledged;
	bool m_terminalBeforeEnd;
};

class TerminalFollowUpHarness {
public:
	TerminalFollowUpHarness(bool * destroyed) :
		m_destroyed(destroyed) {
	}

	~TerminalFollowUpHarness() {
		*m_destroyed = true;
	}

private:
	bool * m_destroyed;
};

int main()
{
	if (!Check(upload_transfer_paths_conflict(_T("C:\\cache\\index.html"), "/site/index.html",
		_T("C:\\cache\\index.html"), "/site/index.html"), "same local and remote paths conflict")) return 1;
	if (!Check(!upload_transfer_paths_conflict(_T("C:\\cache\\index.html"), "/site/index.html",
		_T("C:\\cache\\other.html"), "/site/index.html"), "different local paths do not conflict")) return 1;
	if (!Check(!upload_transfer_paths_conflict(_T("C:\\cache\\index.html"), "/site/index.html",
		_T("C:\\cache\\index.html"), "/site/other.html"), "different remote paths do not conflict")) return 1;

	UploadSchedulingPolicy policy;
	QueueOperation * normalA = FakeOperation(1);
	QueueOperation * normalB = FakeOperation(2);
	QueueOperation * normalC = FakeOperation(3);
	QueueOperation * urgentSave = FakeOperation(4);
	QueueOperation * activeUpload = FakeOperation(5);

	policy.Push(normalA, UploadPriorityNormal);
	if (!Check(policy.TakeNext() == normalA, "normal FIFO first item")) return 1;
	policy.Push(normalB, UploadPriorityNormal);
	policy.Push(normalC, UploadPriorityNormal);
	policy.Push(urgentSave, UploadPriorityUrgent);
	if (!Check(policy.TakeNext() == urgentSave, "urgent before waiting normal")) return 1;

	if (!Check(policy.PromoteWaiting(normalC), "promote waiting normal")) return 1;
	if (!Check(policy.ContainsWaiting(normalC), "promoted item remains waiting")) return 1;
	if (!Check(policy.TakeNext() == normalC, "promoted item dispatches next")) return 1;
	if (!Check(policy.TakeNext() == normalB, "normal FIFO after promoted item")) return 1;
	if (!Check(!policy.PromoteWaiting(activeUpload), "active item cannot be promoted")) return 1;
	if (!Check(!policy.ContainsWaiting(activeUpload), "active item is not waiting")) return 1;
	if (!Check(upload_scheduling_queue_size(policy.GetQueueSize(), 1, 0) == 1, "pending add contributes to queue size")) return 1;

	policy.Push(normalA, UploadPriorityNormal);
	if (!Check(policy.Remove(normalA), "remove waiting item")) return 1;
	if (!Check(policy.TakeNext() == NULL, "removed item does not dispatch")) return 1;

	UploadSchedulingPolicy workerPolicy;
	QueueOperation * workerA = FakeOperation(10);
	QueueOperation * workerB = FakeOperation(11);
	QueueOperation * workerC = FakeOperation(12);
	QueueOperation * urgentAfterSlot = FakeOperation(13);
	int activeWorkers = 0;
	const int workerLimit = 2;

	workerPolicy.Push(workerA, UploadPriorityNormal);
	workerPolicy.Push(workerB, UploadPriorityNormal);
	workerPolicy.Push(workerC, UploadPriorityNormal);
	QueueOperation * dispatchedA = activeWorkers < workerLimit ? workerPolicy.TakeNext() : NULL;
	if (dispatchedA)
		++activeWorkers;
	QueueOperation * dispatchedB = activeWorkers < workerLimit ? workerPolicy.TakeNext() : NULL;
	if (dispatchedB)
		++activeWorkers;
	QueueOperation * blockedThird = activeWorkers < workerLimit ? workerPolicy.TakeNext() : NULL;
	if (!Check(dispatchedA == workerA, "first worker receives first normal item")) return 1;
	if (!Check(dispatchedB == workerB, "second worker receives second normal item")) return 1;
	if (!Check(blockedThird == NULL, "worker limit blocks third item")) return 1;

	workerPolicy.Push(urgentAfterSlot, UploadPriorityUrgent);
	--activeWorkers;
	QueueOperation * dispatchedAfterSlot = activeWorkers < workerLimit ? workerPolicy.TakeNext() : NULL;
	if (!Check(dispatchedAfterSlot == urgentAfterSlot, "urgent item takes released slot")) return 1;
	if (!Check(workerPolicy.TakeNext() == workerC, "queued normal remains after urgent")) return 1;

	UploadSchedulingPolicy conflictPolicy;
	QueueOperation * blockedUrgent = FakeOperation(20);
	QueueOperation * blockedNormal = FakeOperation(21);
	QueueOperation * independentNormal = FakeOperation(22);
	conflictPolicy.Push(blockedNormal, UploadPriorityNormal);
	conflictPolicy.Push(independentNormal, UploadPriorityNormal);
	conflictPolicy.Push(blockedUrgent, UploadPriorityUrgent);
	QueueOperation * independent = conflictPolicy.TakeNextMatching([blockedUrgent, blockedNormal](QueueOperation * op) {
		return op != blockedUrgent && op != blockedNormal;
	});
	if (!Check(independent == independentNormal, "dispatch skips uploads conflicting with active workers")) return 1;
	if (!Check(conflictPolicy.ContainsWaiting(blockedUrgent), "blocked urgent remains queued")) return 1;
	if (!Check(conflictPolicy.ContainsWaiting(blockedNormal), "blocked normal remains queued")) return 1;
	if (!Check(conflictPolicy.TakeNextMatching([](QueueOperation *) { return true; }) == blockedUrgent,
		"unblocked urgent keeps priority")) return 1;
	if (!Check(conflictPolicy.TakeNext() == blockedNormal, "unblocked normal remains FIFO")) return 1;

	RemoteUploadBatch * oppositeOrderBatch = new RemoteUploadBatch(new RemoteUploadPlan, "/site");
	oppositeOrderBatch->InitializeFileCounts(2, 0);
	int completionMarkers = 0;
	if (oppositeOrderBatch->CompleteFileTerminal())
		++completionMarkers; // Worker B finishes before worker A.
	if (!Check(completionMarkers == 0, "first worker terminal cannot emit completion")) return 1;
	if (oppositeOrderBatch->CompleteFileTerminal())
		++completionMarkers;
	if (!Check(completionMarkers == 1, "opposite worker completion emits one marker after both terminals")) return 1;
	if (oppositeOrderBatch->RequestCompletionIfReady())
		++completionMarkers;
	if (!Check(completionMarkers == 1, "later queue activity cannot duplicate completion marker")) return 1;
	oppositeOrderBatch->Release();

	RemoteUploadBatch * cleanupBatch = new RemoteUploadBatch(new RemoteUploadPlan, "/site");
	cleanupBatch->InitializeFileCounts(3, 0);
	BatchFileOperationHarness rejected(cleanupBatch, "/site/rejected.txt");
	BatchFileOperationHarness merged(cleanupBatch, "/site/merged.txt");
	BatchFileOperationHarness shutdown(cleanupBatch, "/site/shutdown.txt");
	int cleanupMarkers = 0;
	if (queue_cancel_and_take_terminal(&rejected)) ++cleanupMarkers;
	if (queue_cancel_and_take_terminal(&merged)) ++cleanupMarkers;
	if (queue_cancel_and_take_terminal(&shutdown)) ++cleanupMarkers;
	if (!Check(cleanupMarkers == 1, "rejection merge and shutdown emit one completion")) return 1;
	if (!Check(cleanupBatch->GetRemainingFileCount() == 0, "cleanup paths reach batch terminal")) return 1;
	if (!Check(cleanupBatch->GetCanceledFileCount() == 3, "cleanup paths are canceled")) return 1;
	cleanupBatch->Release();

	RemoteUploadBatch * acknowledgedBatch = new RemoteUploadBatch(new RemoteUploadPlan, "/site");
	acknowledgedBatch->InitializeFileCounts(1, 0);
	BatchFileOperationHarness acknowledged(acknowledgedBatch, "/site/ack.txt");
	if (!Check(queue_end_and_take_terminal(&acknowledged, 2), "End acknowledgement releases completion")) return 1;
	if (!Check(!acknowledged.TerminalRanBeforeEnd(), "terminal hook runs after End acknowledgement")) return 1;
	acknowledgedBatch->Release();

	RemoteUploadBatch * abortBatch = new RemoteUploadBatch(new RemoteUploadPlan, "/site");
	abortBatch->InitializeFileCounts(1, 0);
	BatchFileOperationHarness aborted(abortBatch, "/site/aborted.txt");
	aborted.OnQueueCanceled();
	if (!Check(resolve_remote_upload_file_outcome(-1, aborted.WasCanceled()) == RemoteUploadFileCanceled,
		"active Abort is canceled")) return 1;
	if (!Check(resolve_remote_upload_file_outcome(-1, false) == RemoteUploadFileFailed,
		"server failure remains failed")) return 1;
	if (!Check(aborted.OnQueueTerminal(), "active Abort reaches terminal completion")) return 1;
	abortBatch->Release();

	RemoteUploadBatch * zeroSelected = new RemoteUploadBatch(new RemoteUploadPlan, "/site");
	zeroSelected->InitializeFileCounts(0, 2);
	if (!Check(zeroSelected->RequestCompletionIfReady(), "zero-selected batch requests completion")) return 1;
	if (!Check(!zeroSelected->RequestCompletionIfReady(), "zero-selected completion is one-shot")) return 1;
	zeroSelected->Release();

	bool shutdownMarkerDestroyed = false;
	TerminalFollowUpHarness * shutdownMarker = new TerminalFollowUpHarness(&shutdownMarkerDestroyed);
	if (!Check(queue_filter_terminal_follow_up(shutdownMarker, true) == NULL,
		"session teardown suppresses completion marker")) return 1;
	if (!Check(shutdownMarkerDestroyed, "suppressed completion marker is released")) return 1;

	bool normalMarkerDestroyed = false;
	TerminalFollowUpHarness * normalMarker = new TerminalFollowUpHarness(&normalMarkerDestroyed);
	if (!Check(queue_filter_terminal_follow_up(normalMarker, false) == normalMarker,
		"normal operation preserves completion marker")) return 1;
	if (!Check(!normalMarkerDestroyed, "normal completion marker remains owned by caller")) return 1;
	delete normalMarker;

	printf("concurrent_upload_scheduler_exit=0\n");
	return 0;
}
