#include "src/StdInc.h"
#include "src/ConcurrentUploadScheduler.h"
#include "src/FTPQueue.h"
#include "src/RemoteUploadPlan.h"

#include <stdio.h>

Output * _MainOutput = NULL;
HWND _MainOutputWindow = NULL;

int OutputDebug(const TCHAR *, ...) { return 0; }
int OutputMsg(const TCHAR *, ...) { return 0; }
int OutputClnt(const TCHAR *, ...) { return 0; }
int OutputErr(const TCHAR *, ...) { return 0; }
int MessageBoxOutput(const TCHAR *) { return 0; }

static bool Check(bool condition, const char * message)
{
	if (condition)
		return true;
	fprintf(stderr, "ftp_queue_terminal_lifecycle_failed=%s\n", message);
	return false;
}

class FakeWrapper : public FTPClientWrapper {
public:
	FakeWrapper() :
		FTPClientWrapper(Client_SSH, "test", 22, "user", "password"),
		m_sendStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		m_sendRelease(CreateEvent(NULL, TRUE, TRUE, NULL)),
		m_mkdirStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		m_mkdirRelease(CreateEvent(NULL, TRUE, TRUE, NULL)),
		m_abortCount(0) {
	}

	~FakeWrapper() {
		CloseHandle(m_sendStarted);
		CloseHandle(m_sendRelease);
		CloseHandle(m_mkdirStarted);
		CloseHandle(m_mkdirRelease);
	}

	void BlockSend() { ResetEvent(m_sendRelease); }
	void BlockMkdir() { ResetEvent(m_mkdirRelease); }
	HANDLE SendStarted() const { return m_sendStarted; }
	HANDLE MkdirStarted() const { return m_mkdirStarted; }
	int AbortCount() const { return static_cast<int>(InterlockedCompareExchange(const_cast<volatile LONG*>(&m_abortCount), 0, 0)); }

	virtual FTPClientWrapper * Clone() { return new FakeWrapper; }
	virtual int Connect() { m_connected = true; return 0; }
	virtual int Disconnect() { m_connected = false; return 0; }
	virtual int NoOp() { return 0; }
	virtual int GetDir(const char *, FTPFile **) { return -1; }
	virtual int Cwd(const char *) { return 0; }
	virtual int Pwd(char *, size_t) { return 0; }
	virtual int Rename(const char *, const char *) { return 0; }
	virtual int ChmodFile(const char *, const char *) { return 0; }
	virtual int MkDir(const char *) {
		SetEvent(m_mkdirStarted);
		WaitForSingleObject(m_mkdirRelease, INFINITE);
		return 0;
	}
	virtual int RmDir(const char *) { return 0; }
	virtual int MkFile(const char *) { return 0; }
	virtual int SendFile(const TCHAR *, const char *) {
		SetEvent(m_sendStarted);
		WaitForSingleObject(m_sendRelease, INFINITE);
		return 0;
	}
	virtual int ReceiveFile(const TCHAR *, const char *) { return 0; }
	virtual int SendFile(HANDLE, const char *) { return 0; }
	virtual int ReceiveFile(HANDLE, const char *) { return 0; }
	virtual int DeleteFile(const char *) { return 0; }
	virtual DWORD LastAction() { return 0; }
	virtual int Abort() {
		InterlockedIncrement(&m_abortCount);
		SetEvent(m_sendRelease);
		SetEvent(m_mkdirRelease);
		return 0;
	}

private:
	HANDLE m_sendStarted;
	HANDLE m_sendRelease;
	HANDLE m_mkdirStarted;
	HANDLE m_mkdirRelease;
	volatile LONG m_abortCount;
};

struct WindowHarness {
	WindowHarness() : generation(1), markerQueue(NULL), lateAbortQueue(NULL),
		lateAbortOperation(NULL), lateAbortResult(99), completionPosts(0),
		droppedPosts(0), markerEnds(0), prepareDispatches(0) {
	}

	volatile LONG generation;
	FTPQueue * markerQueue;
	FTPQueue * lateAbortQueue;
	QueueOperation * lateAbortOperation;
	int lateAbortResult;
	volatile LONG completionPosts;
	volatile LONG droppedPosts;
	volatile LONG markerEnds;
	volatile LONG prepareDispatches;
};

static LRESULT CALLBACK HarnessWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowHarness * harness = reinterpret_cast<WindowHarness*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (message == WM_NCCREATE) {
		CREATESTRUCT * create = reinterpret_cast<CREATESTRUCT*>(lParam);
		harness = static_cast<WindowHarness*>(create->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(harness));
		return TRUE;
	}
	if (message == NotifyMessageRemoteUploadBatchComplete) {
		RemoteUploadBatch * batch = reinterpret_cast<RemoteUploadBatch*>(lParam);
		InterlockedIncrement(&harness->completionPosts);
		if (static_cast<LONG>(wParam) == InterlockedCompareExchange(&harness->generation, 0, 0) && harness->markerQueue)
			harness->markerQueue->AddQueueOp(new QueueRemoteUploadComplete(hwnd, batch));
		else
			InterlockedIncrement(&harness->droppedPosts);
		batch->Release();
		return 0;
	}
	if (message < NotifyMessageMIN || message > NotifyMessageMAX)
		return DefWindowProc(hwnd, message, wParam, lParam);

	QueueOperation * operation = reinterpret_cast<QueueOperation*>(lParam);
	if (message == NotifyMessageEnd) {
		if (operation == harness->lateAbortOperation)
			harness->lateAbortResult = harness->lateAbortQueue->AbortActive();
		if (operation->GetType() == QueueOperation::QueueTypeUpload) {
			RemoteUploadBatch * batch = static_cast<RemoteUploadBatch*>(operation->GetNotifyData());
			RemoteUploadFileOutcome outcome = resolve_remote_upload_file_outcome(operation->GetResult(), operation->WasCanceled());
			if (outcome == RemoteUploadFileSucceeded)
				batch->RecordFileSucceeded();
			else if (outcome == RemoteUploadFileFailed)
				batch->RecordFileFailed();
		} else if (operation->GetType() == QueueOperation::QueueTypeRemoteUploadPrepare) {
			if (should_dispatch_remote_upload_after_prepare(operation->GetResult(), operation->WasCanceled()))
				InterlockedIncrement(&harness->prepareDispatches);
		} else if (operation->GetType() == QueueOperation::QueueTypeRemoteUploadComplete) {
			InterlockedIncrement(&harness->markerEnds);
		}
	}
	operation->AckNotification();
	return 0;
}

static HWND CreateHarnessWindow(WindowHarness * harness)
{
	WNDCLASS windowClass = {};
	windowClass.lpfnWndProc = HarnessWindowProc;
	windowClass.hInstance = GetModuleHandle(NULL);
	windowClass.lpszClassName = TEXT("NppFTPQueueLifecycleHarness");
	RegisterClass(&windowClass);
	return CreateWindow(windowClass.lpszClassName, TEXT(""), 0, 0, 0, 0, 0,
		HWND_MESSAGE, NULL, windowClass.hInstance, harness);
}

static void PumpMessages()
{
	MSG message;
	while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}

static bool PumpUntil(volatile LONG * value, LONG expected, DWORD timeoutMs)
{
	DWORD started = GetTickCount();
	while (InterlockedCompareExchange(value, 0, 0) != expected) {
		PumpMessages();
		if (GetTickCount() - started >= timeoutMs)
			return false;
		Sleep(1);
	}
	PumpMessages();
	return true;
}

static bool PumpUntilEvent(HANDLE eventHandle, DWORD timeoutMs)
{
	DWORD started = GetTickCount();
	while (WaitForSingleObject(eventHandle, 0) != WAIT_OBJECT_0) {
		PumpMessages();
		if (GetTickCount() - started >= timeoutMs)
			return false;
		Sleep(1);
	}
	return true;
}

static RemoteUploadBatch * MakeFileBatch(HWND hwnd, LONG generation)
{
	RemoteUploadPlan * plan = new RemoteUploadPlan;
	plan->AddFile(TEXT("local.txt"), "/site/local.txt");
	RemoteUploadBatch * batch = new RemoteUploadBatch(plan, "/site", hwnd, generation);
	batch->InitializeFileCounts(1, 0);
	return batch;
}

static bool TestNormalAndLateAbort(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper workerWrapper;
	FakeWrapper markerWrapper;
	FTPQueue markerQueue(&markerWrapper);
	FTPQueue workerQueue(&workerWrapper);
	harness->markerQueue = &markerQueue;
	if (!Check(markerQueue.Initialize() == 0 && workerQueue.Initialize() == 0, "initialize normal queues")) return false;
	RemoteUploadBatch * batch = MakeFileBatch(hwnd, harness->generation);
	QueueRemoteUploadFile * upload = new QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, batch);
	harness->lateAbortQueue = &workerQueue;
	harness->lateAbortOperation = upload;
	LONG markerBefore = harness->markerEnds;
	workerQueue.AddQueueOp(upload);
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000), "normal completion marker")) return false;
	if (!Check(harness->lateAbortResult == 0 && workerWrapper.AbortCount() == 0, "late Abort is a no-op")) return false;
	if (!Check(batch->completedCount == 1 && batch->GetCanceledFileCount() == 0 && batch->GetRemainingFileCount() == 0,
		"late Abort preserves success")) return false;
	harness->lateAbortQueue = NULL;
	harness->lateAbortOperation = NULL;
	batch->Release();
	workerQueue.Deinitialize();
	markerQueue.Deinitialize();
	return true;
}

static bool TestActiveAbort(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper workerWrapper;
	workerWrapper.BlockSend();
	FakeWrapper markerWrapper;
	FTPQueue markerQueue(&markerWrapper);
	FTPQueue workerQueue(&workerWrapper);
	harness->markerQueue = &markerQueue;
	markerQueue.Initialize();
	workerQueue.Initialize();
	RemoteUploadBatch * batch = MakeFileBatch(hwnd, harness->generation);
	LONG markerBefore = harness->markerEnds;
	workerQueue.AddQueueOp(new QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, batch));
	if (!Check(PumpUntilEvent(workerWrapper.SendStarted(), 5000), "upload enters SendFile")) return false;
	if (!Check(workerQueue.AbortActive() == 0, "active Abort succeeds")) return false;
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000), "active Abort completion marker")) return false;
	if (!Check(batch->GetCanceledFileCount() == 1 && batch->GetFailedFileCount() == 0 && workerWrapper.AbortCount() == 1,
		"active Abort is canceled, not failed")) return false;
	batch->Release();
	workerQueue.Deinitialize();
	markerQueue.Deinitialize();
	return true;
}

static bool TestPrepareAbort(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper workerWrapper;
	workerWrapper.BlockMkdir();
	FakeWrapper markerWrapper;
	FTPQueue markerQueue(&markerWrapper);
	FTPQueue workerQueue(&workerWrapper);
	harness->markerQueue = &markerQueue;
	markerQueue.Initialize();
	workerQueue.Initialize();
	RemoteUploadPlan * plan = new RemoteUploadPlan;
	plan->AddDirectory(TEXT("folder"), "/site/folder");
	plan->AddFile(TEXT("folder\\file.txt"), "/site/folder/file.txt");
	RemoteUploadBatch * batch = new RemoteUploadBatch(plan, "/site", hwnd, harness->generation);
	batch->InitializeFileCounts(1, 0);
	LONG markerBefore = harness->markerEnds;
	LONG dispatchBefore = harness->prepareDispatches;
	workerQueue.AddQueueOp(new QueueRemoteUploadPrepare(hwnd, batch));
	if (!Check(PumpUntilEvent(workerWrapper.MkdirStarted(), 5000), "prepare enters MkDir")) return false;
	if (!Check(workerQueue.AbortActive() == 0, "prepare Abort succeeds")) return false;
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000), "prepare Abort completion marker")) return false;
	if (!Check(harness->prepareDispatches == dispatchBefore && batch->GetCanceledFileCount() == 1 &&
		batch->GetRemainingFileCount() == 0, "prepare Abort cancels without dispatch")) return false;
	batch->Release();
	workerQueue.Deinitialize();
	markerQueue.Deinitialize();
	return true;
}

static bool TestTeardownSuppression(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper workerWrapper;
	workerWrapper.BlockSend();
	FTPQueue workerQueue(&workerWrapper);
	workerQueue.Initialize();
	RemoteUploadBatch * batch = MakeFileBatch(hwnd, harness->generation);
	LONG postsBefore = harness->completionPosts;
	LONG markersBefore = harness->markerEnds;
	LONG dropsBefore = harness->droppedPosts;
	workerQueue.AddQueueOp(new QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, batch));
	if (!Check(PumpUntilEvent(workerWrapper.SendStarted(), 5000), "teardown upload enters SendFile")) return false;
	InterlockedIncrement(&harness->generation);
	if (!Check(workerQueue.BeginTeardown() == 0, "teardown aborts active transfer")) return false;
	workerQueue.Deinitialize();
	if (!Check(PumpUntil(&harness->completionPosts, postsBefore + 1, 5000), "teardown completion payload posted")) return false;
	if (!Check(harness->droppedPosts == dropsBefore + 1 && harness->markerEnds == markersBefore,
		"invalid generation drops summary marker")) return false;
	if (!Check(batch->GetCanceledFileCount() == 1 && batch->GetRemainingFileCount() == 0,
		"teardown still accounts terminal cancellation")) return false;
	batch->Release();
	return true;
}

static bool TestRejectionAndZeroSelected(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper markerWrapper;
	FTPQueue markerQueue(&markerWrapper);
	harness->markerQueue = &markerQueue;
	markerQueue.Initialize();

	FakeWrapper rejectedWrapper;
	ConcurrentUploadScheduler rejectedScheduler(hwnd, &rejectedWrapper, 1, &markerQueue);
	RemoteUploadBatch * rejected = MakeFileBatch(hwnd, harness->generation);
	LONG markerBefore = harness->markerEnds;
	if (!Check(rejectedScheduler.AddQueueOp(new QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, rejected),
		UploadPriorityNormal) == -1, "stopped scheduler rejects upload")) return false;
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000), "rejected upload completion marker")) return false;
	if (!Check(rejected->GetCanceledFileCount() == 1 && rejected->GetRemainingFileCount() == 0,
		"rejected upload reaches terminal accounting")) return false;
	rejected->Release();

	RemoteUploadBatch * empty = new RemoteUploadBatch(new RemoteUploadPlan, "/site", hwnd, harness->generation);
	empty->InitializeFileCounts(0, 2);
	markerBefore = harness->markerEnds;
	if (!Check(empty->RequestCompletionIfReady() && !empty->RequestCompletionIfReady(), "zero-selected request is exactly once")) return false;
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000), "zero-selected completion marker")) return false;
	empty->Release();
	markerQueue.Deinitialize();
	return true;
}

int main()
{
	WindowHarness harness;
	HWND hwnd = CreateHarnessWindow(&harness);
	if (!Check(hwnd != NULL, "create message-ack window")) return 1;
	if (!TestNormalAndLateAbort(hwnd, &harness)) return 1;
	if (!TestActiveAbort(hwnd, &harness)) return 1;
	if (!TestPrepareAbort(hwnd, &harness)) return 1;
	if (!TestTeardownSuppression(hwnd, &harness)) return 1;
	if (!TestRejectionAndZeroSelected(hwnd, &harness)) return 1;
	DestroyWindow(hwnd);
	printf("ftp_queue_terminal_lifecycle_exit=0\n");
	return 0;
}
