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

struct FakeWrapperState {
	FakeWrapperState() :
		sendStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		sendRelease(CreateEvent(NULL, TRUE, TRUE, NULL)),
		firstSendStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		firstSendRelease(CreateEvent(NULL, TRUE, FALSE, NULL)),
		secondSendStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		secondSendRelease(CreateEvent(NULL, TRUE, FALSE, NULL)),
		mkdirStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		mkdirFinished(CreateEvent(NULL, TRUE, FALSE, NULL)),
		mkdirRelease(CreateEvent(NULL, TRUE, TRUE, NULL)),
		abortCount(0),
		sendCallCount(0) {
	}

	~FakeWrapperState() {
		CloseHandle(sendStarted);
		CloseHandle(sendRelease);
		CloseHandle(firstSendStarted);
		CloseHandle(firstSendRelease);
		CloseHandle(secondSendStarted);
		CloseHandle(secondSendRelease);
		CloseHandle(mkdirStarted);
		CloseHandle(mkdirFinished);
		CloseHandle(mkdirRelease);
	}

	HANDLE sendStarted;
	HANDLE sendRelease;
	HANDLE firstSendStarted;
	HANDLE firstSendRelease;
	HANDLE secondSendStarted;
	HANDLE secondSendRelease;
	HANDLE mkdirStarted;
	HANDLE mkdirFinished;
	HANDLE mkdirRelease;
	volatile LONG abortCount;
	volatile LONG sendCallCount;
};

class FakeWrapper : public FTPClientWrapper {
public:
	FakeWrapper(FakeWrapperState * sharedState = NULL) :
		FTPClientWrapper(Client_SSH, "test", 22, "user", "password"),
		m_ownedState(sharedState ? NULL : new FakeWrapperState),
		m_state(sharedState ? sharedState : m_ownedState) {
	}

	~FakeWrapper() {
		delete m_ownedState;
	}

	void BlockSend() { ResetEvent(m_state->sendRelease); }
	void BlockMkdir() { ResetEvent(m_state->mkdirRelease); }
	HANDLE SendStarted() const { return m_state->sendStarted; }
	HANDLE MkdirStarted() const { return m_state->mkdirStarted; }
	HANDLE MkdirFinished() const { return m_state->mkdirFinished; }
	HANDLE FirstSendStarted() const { return m_state->firstSendStarted; }
	HANDLE SecondSendStarted() const { return m_state->secondSendStarted; }
	void ReleaseFirstSend() { SetEvent(m_state->firstSendRelease); }
	void ReleaseSecondSend() { SetEvent(m_state->secondSendRelease); }
	int AbortCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->abortCount, 0, 0)); }
	int SendCallCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->sendCallCount, 0, 0)); }

	virtual FTPClientWrapper * Clone() { return new FakeWrapper(m_state); }
	virtual int Connect() { m_connected = true; return 0; }
	virtual int Disconnect() { m_connected = false; return 0; }
	virtual int NoOp() { return 0; }
	virtual int GetDir(const char *, FTPFile **) { return -1; }
	virtual int Cwd(const char *) { return 0; }
	virtual int Pwd(char *, size_t) { return 0; }
	virtual int Rename(const char *, const char *) { return 0; }
	virtual int ChmodFile(const char *, const char *) { return 0; }
	virtual int MkDir(const char *) {
		SetEvent(m_state->mkdirStarted);
		WaitForSingleObject(m_state->mkdirRelease, INFINITE);
		SetEvent(m_state->mkdirFinished);
		return 0;
	}
	virtual int RmDir(const char *) { return 0; }
	virtual int MkFile(const char *) { return 0; }
	virtual int SendFile(const TCHAR *, const char * remotePath) {
		InterlockedIncrement(&m_state->sendCallCount);
		if (remotePath && strstr(remotePath, "first.txt")) {
			SetEvent(m_state->firstSendStarted);
			WaitForSingleObject(m_state->firstSendRelease, INFINITE);
			return 0;
		}
		if (remotePath && strstr(remotePath, "second.txt")) {
			SetEvent(m_state->secondSendStarted);
			WaitForSingleObject(m_state->secondSendRelease, INFINITE);
			return 0;
		}
		SetEvent(m_state->sendStarted);
		WaitForSingleObject(m_state->sendRelease, INFINITE);
		return 0;
	}
	virtual int ReceiveFile(const TCHAR *, const char *) { return 0; }
	virtual int SendFile(HANDLE, const char *) { return 0; }
	virtual int ReceiveFile(HANDLE, const char *) { return 0; }
	virtual int DeleteFile(const char *) { return 0; }
	virtual DWORD LastAction() { return 0; }
	virtual int Abort() {
		InterlockedIncrement(&m_state->abortCount);
		SetEvent(m_state->sendRelease);
		SetEvent(m_state->firstSendRelease);
		SetEvent(m_state->secondSendRelease);
		SetEvent(m_state->mkdirRelease);
		return 0;
	}

private:
	FakeWrapperState * m_ownedState;
	FakeWrapperState * m_state;
};

class BlockingRemoteUploadComplete : public QueueRemoteUploadComplete {
public:
	BlockingRemoteUploadComplete(HWND hwnd, RemoteUploadBatch * batch) :
		QueueRemoteUploadComplete(hwnd, batch),
		m_started(CreateEvent(NULL, TRUE, FALSE, NULL)),
		m_release(CreateEvent(NULL, TRUE, FALSE, NULL)) {
	}

	~BlockingRemoteUploadComplete() {
		CloseHandle(m_started);
		CloseHandle(m_release);
	}

	virtual int Perform() {
		m_result = 0;
		SetEvent(m_started);
		WaitForSingleObject(m_release, INFINITE);
		return m_result;
	}

	HANDLE Started() const { return m_started; }
	HANDLE ReleaseEvent() const { return m_release; }

private:
	HANDLE m_started;
	HANDLE m_release;
};

class FinalizedBlockingUpload : public QueueRemoteUploadFile {
public:
	FinalizedBlockingUpload(HWND hwnd, RemoteUploadBatch * batch, int result) :
		QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, batch),
		m_forcedResult(result),
		m_finalized(CreateEvent(NULL, TRUE, FALSE, NULL)),
		m_release(CreateEvent(NULL, TRUE, FALSE, NULL)) {
	}

	~FinalizedBlockingUpload() {
		CloseHandle(m_finalized);
		CloseHandle(m_release);
	}

	virtual int Perform() {
		CompletePerform(m_forcedResult);
		SetEvent(m_finalized);
		WaitForSingleObject(m_release, INFINITE);
		return m_result;
	}

	HANDLE Finalized() const { return m_finalized; }
	void ReleasePerform() { SetEvent(m_release); }

private:
	int m_forcedResult;
	HANDLE m_finalized;
	HANDLE m_release;
};

struct WindowHarness {
	WindowHarness() : generation(1), markerQueue(NULL), lateAbortQueue(NULL),
		lateAbortOperation(NULL), lateAbortResult(99), completionPosts(0),
		droppedPosts(0), markerEnds(0), prepareDispatches(0), uploadStarts(0) {
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
	volatile LONG uploadStarts;
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
	if (message == NotifyMessageStart && operation->GetType() == QueueOperation::QueueTypeUpload)
		InterlockedIncrement(&harness->uploadStarts);
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

static void PumpFor(DWORD durationMs)
{
	DWORD started = GetTickCount();
	while (GetTickCount() - started < durationMs) {
		PumpMessages();
		Sleep(1);
	}
}

static bool WaitForQueuedMessage(HWND hwnd, UINT targetMessage, DWORD timeoutMs)
{
	DWORD started = GetTickCount();
	MSG message;
	while (!PeekMessage(&message, hwnd, targetMessage, targetMessage, PM_NOREMOVE)) {
		if (GetTickCount() - started >= timeoutMs)
			return false;
		Sleep(1);
	}
	return true;
}

struct DelayedSignal {
	HANDLE eventHandle;
	DWORD delayMs;
};

static DWORD WINAPI SignalAfterDelay(LPVOID parameter)
{
	DelayedSignal * signal = static_cast<DelayedSignal*>(parameter);
	Sleep(signal->delayMs);
	SetEvent(signal->eventHandle);
	return 0;
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

static bool TestFinalizedResultRejectsLateAbort(HWND hwnd, WindowHarness * harness, int result)
{
	FakeWrapper workerWrapper;
	FakeWrapper markerWrapper;
	FTPQueue markerQueue(&markerWrapper);
	FTPQueue workerQueue(&workerWrapper);
	harness->markerQueue = &markerQueue;
	if (!Check(markerQueue.Initialize() == 0 && workerQueue.Initialize() == 0, "initialize finalized-result queues")) return false;

	RemoteUploadBatch * batch = MakeFileBatch(hwnd, harness->generation);
	FinalizedBlockingUpload * upload = new FinalizedBlockingUpload(hwnd, batch, result);
	LONG markerBefore = harness->markerEnds;
	workerQueue.AddQueueOp(upload);
	if (!Check(PumpUntilEvent(upload->Finalized(), 5000), "upload result reaches finalization point")) return false;
	workerQueue.AbortActive();
	upload->ReleasePerform();
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000), "finalized upload reaches completion marker")) return false;
	if (!Check(workerWrapper.AbortCount() == 0 && batch->GetCanceledFileCount() == 0,
		"late Abort cannot relabel a finalized result")) return false;
	if (result == 0) {
		if (!Check(batch->completedCount == 1 && batch->GetFailedFileCount() == 0,
			"finalized success remains successful")) return false;
	} else if (!Check(batch->completedCount == 0 && batch->GetFailedFileCount() == 1,
		"finalized server failure remains failed")) return false;

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

static bool TestSchedulerIdleShutdown(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper prototype;
	ConcurrentUploadScheduler scheduler(hwnd, &prototype, 1, NULL);
	harness->markerQueue = NULL;
	if (!Check(scheduler.Initialize() == 0, "initialize scheduler for idle shutdown")) return false;

	RemoteUploadBatch * batch = MakeFileBatch(hwnd, harness->generation);
	LONG startsBefore = harness->uploadStarts;
	if (!Check(scheduler.AddQueueOp(new QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, batch),
		UploadPriorityNormal) == 0, "queue scheduler idle-shutdown upload")) return false;
	if (!Check(PumpUntil(&harness->uploadStarts, startsBefore + 1, 5000), "scheduler worker reaches pre-execution idle state")) return false;
	if (!Check(scheduler.Deinitialize() == 0, "deinitialize scheduler from idle state")) return false;
	PumpMessages();
	if (!Check(prototype.SendCallCount() == 0, "idle scheduler shutdown never calls SendFile")) return false;
	if (!Check(batch->GetCanceledFileCount() == 1 && batch->GetRemainingFileCount() == 0,
		"idle scheduler shutdown terminal-accounts upload")) return false;
	batch->Release();
	return true;
}

static bool TestSchedulerOppositeCompletionOrder(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper prototype;
	FakeWrapper markerWrapper;
	FTPQueue markerQueue(&markerWrapper);
	ConcurrentUploadScheduler scheduler(hwnd, &prototype, 2, &markerQueue);
	harness->markerQueue = &markerQueue;
	if (!Check(markerQueue.Initialize() == 0 && scheduler.Initialize() == 0,
		"initialize two-worker scheduler")) return false;

	RemoteUploadPlan * plan = new RemoteUploadPlan;
	plan->AddFile(TEXT("first.txt"), "/site/first.txt");
	plan->AddFile(TEXT("second.txt"), "/site/second.txt");
	RemoteUploadBatch * batch = new RemoteUploadBatch(plan, "/site", hwnd, harness->generation);
	batch->InitializeFileCounts(2, 0);
	LONG markerBefore = harness->markerEnds;
	scheduler.AddQueueOp(new QueueRemoteUploadFile(hwnd, "/site/first.txt", TEXT("first.txt"), Mode_Binary, batch), UploadPriorityNormal);
	scheduler.AddQueueOp(new QueueRemoteUploadFile(hwnd, "/site/second.txt", TEXT("second.txt"), Mode_Binary, batch), UploadPriorityNormal);
	if (!Check(PumpUntilEvent(prototype.FirstSendStarted(), 5000) &&
		PumpUntilEvent(prototype.SecondSendStarted(), 5000), "two scheduler workers enter SendFile")) return false;

	prototype.ReleaseSecondSend();
	PumpFor(250);
	if (!Check(batch->GetRemainingFileCount() == 1 && harness->markerEnds == markerBefore,
		"second worker terminal cannot complete the batch early")) return false;
	prototype.ReleaseFirstSend();
	if (!Check(PumpUntil(&harness->markerEnds, markerBefore + 1, 5000),
		"opposite worker order emits one final marker")) return false;
	PumpFor(100);
	if (!Check(batch->completedCount == 2 && batch->GetRemainingFileCount() == 0 &&
		harness->markerEnds == markerBefore + 1, "real scheduler path completes the batch exactly once")) return false;

	batch->Release();
	scheduler.Deinitialize();
	markerQueue.Deinitialize();
	return true;
}

static bool TestFinishedPrepareTeardown(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper wrapper;
	FTPQueue queue(&wrapper);
	harness->markerQueue = NULL;
	if (!Check(queue.Initialize() == 0, "initialize finished-prepare queue")) return false;

	RemoteUploadPlan * plan = new RemoteUploadPlan;
	plan->AddDirectory(TEXT("folder"), "/site/folder");
	plan->AddFile(TEXT("folder\\file.txt"), "/site/folder/file.txt");
	RemoteUploadBatch * batch = new RemoteUploadBatch(plan, "/site", hwnd, harness->generation);
	batch->InitializeFileCounts(1, 0);
	queue.AddQueueOp(new QueueRemoteUploadPrepare(hwnd, batch));
	if (!Check(PumpUntilEvent(wrapper.MkdirFinished(), 5000), "prepare Perform finishes")) return false;
	if (!Check(WaitForQueuedMessage(hwnd, NotifyMessageEnd, 5000), "prepare End waits for UI dispatch")) return false;

	InterlockedIncrement(&harness->generation);
	if (!Check(queue.BeginTeardown() == 0, "begin teardown with finished prepare")) return false;
	queue.Deinitialize();
	PumpMessages();
	if (!Check(batch->GetCanceledFileCount() == 1 && batch->GetRemainingFileCount() == 0,
		"finished undispatched prepare terminal-accounts selected files")) return false;
	batch->Release();
	return true;
}

static bool TestActiveCompletionMarkerTeardown(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper wrapper;
	FTPQueue queue(&wrapper);
	if (!Check(queue.Initialize() == 0, "initialize active-marker queue")) return false;

	RemoteUploadBatch * batch = new RemoteUploadBatch(new RemoteUploadPlan, "/site");
	BlockingRemoteUploadComplete * marker = new BlockingRemoteUploadComplete(hwnd, batch);
	LONG markerEndsBefore = harness->markerEnds;
	queue.AddQueueOp(marker);
	if (!Check(PumpUntilEvent(marker->Started(), 5000), "completion marker enters Perform")) return false;

	DelayedSignal delayed = { marker->ReleaseEvent(), 150 };
	HANDLE signalThread = CreateThread(NULL, 0, &SignalAfterDelay, &delayed, 0, NULL);
	if (!Check(signalThread != NULL, "create completion-marker release thread")) return false;
	queue.BeginTeardown();
	queue.Deinitialize();
	WaitForSingleObject(signalThread, INFINITE);
	CloseHandle(signalThread);
	PumpMessages();
	if (!Check(harness->markerEnds == markerEndsBefore, "confirmed teardown suppresses active completion summary")) return false;
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
	if (!TestFinalizedResultRejectsLateAbort(hwnd, &harness, 0)) return 1;
	if (!TestFinalizedResultRejectsLateAbort(hwnd, &harness, -1)) return 1;
	if (!TestPrepareAbort(hwnd, &harness)) return 1;
	if (!TestTeardownSuppression(hwnd, &harness)) return 1;
	if (!TestSchedulerIdleShutdown(hwnd, &harness)) return 1;
	if (!TestSchedulerOppositeCompletionOrder(hwnd, &harness)) return 1;
	if (!TestFinishedPrepareTeardown(hwnd, &harness)) return 1;
	if (!TestActiveCompletionMarkerTeardown(hwnd, &harness)) return 1;
	if (!TestRejectionAndZeroSelected(hwnd, &harness)) return 1;
	DestroyWindow(hwnd);
	printf("ftp_queue_terminal_lifecycle_exit=0\n");
	return 0;
}
