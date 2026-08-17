#include "CertificateStoreLock.h"

#include <windows.h>
#include <stdio.h>

struct WorkerContext {
	int * value;
	int iterations;
};

static DWORD WINAPI IncrementWorker(LPVOID parameter)
{
	WorkerContext * context = static_cast<WorkerContext *>(parameter);
	for (int i = 0; i < context->iterations; ++i) {
		CertificateStoreLock lock;
		int current = *context->value;
		::Sleep(0);
		*context->value = current + 1;
	}
	return 0;
}

int main()
{
	const int workerCount = 8;
	const int iterations = 2000;
	int value = 0;
	WorkerContext context = {&value, iterations};
	HANDLE workers[workerCount]{};

	for (int i = 0; i < workerCount; ++i) {
		workers[i] = ::CreateThread(NULL, 0, &IncrementWorker, &context, 0, NULL);
		if (!workers[i]) {
			fprintf(stderr, "certificate_store_lock_failed=create_thread\n");
			return 1;
		}
	}

	DWORD waitResult = ::WaitForMultipleObjects(workerCount, workers, TRUE, INFINITE);
	for (int i = 0; i < workerCount; ++i)
		::CloseHandle(workers[i]);

	if (waitResult != WAIT_OBJECT_0 || value != workerCount * iterations) {
		fprintf(stderr, "certificate_store_lock_failed=value_%d\n", value);
		return 1;
	}

	printf("certificate_store_lock_exit=0\n");
	return 0;
}
