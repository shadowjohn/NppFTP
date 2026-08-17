// Focused command:
// cl /nologo /EHsc /I src tests\concurrent_upload_scheduler.cpp /Fe:_build\tests\concurrent_upload_scheduler.exe

#include "UploadSchedulingPolicy.h"
#include "UploadTransferIdentity.h"

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

	printf("concurrent_upload_scheduler_exit=0\n");
	return 0;
}
