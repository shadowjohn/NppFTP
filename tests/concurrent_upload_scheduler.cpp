// Focused command:
// cl /nologo /EHsc /I src tests\concurrent_upload_scheduler.cpp /Fe:_build\tests\concurrent_upload_scheduler.exe

#include "UploadSchedulingPolicy.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static QueueOperation * FakeOperation(unsigned int value)
{
	return reinterpret_cast<QueueOperation *>(static_cast<uintptr_t>(value));
}

int main()
{
	UploadSchedulingPolicy policy;
	QueueOperation * normalA = FakeOperation(1);
	QueueOperation * normalB = FakeOperation(2);
	QueueOperation * normalC = FakeOperation(3);
	QueueOperation * urgentSave = FakeOperation(4);
	QueueOperation * activeUpload = FakeOperation(5);

	policy.Push(normalA, UploadPriorityNormal);
	assert(policy.TakeNext() == normalA);
	policy.Push(normalB, UploadPriorityNormal);
	policy.Push(normalC, UploadPriorityNormal);
	policy.Push(urgentSave, UploadPriorityUrgent);
	assert(policy.TakeNext() == urgentSave);

	assert(policy.PromoteWaiting(normalC) == true);
	assert(policy.ContainsWaiting(normalC) == true);
	assert(policy.TakeNext() == normalC);
	assert(policy.TakeNext() == normalB);
	assert(policy.PromoteWaiting(activeUpload) == false);
	assert(policy.ContainsWaiting(activeUpload) == false);

	policy.Push(normalA, UploadPriorityNormal);
	assert(policy.Remove(normalA) == true);
	assert(policy.TakeNext() == NULL);

	printf("concurrent_upload_scheduler_exit=0\n");
	return 0;
}
