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
		waitingBlockStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		waitingBlockRelease(CreateEvent(NULL, TRUE, FALSE, NULL)),
		activeSaveStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		activeSaveRelease(CreateEvent(NULL, TRUE, FALSE, NULL)),
		activeProbeStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		activeProbeRelease(CreateEvent(NULL, TRUE, FALSE, NULL)),
		mkdirStarted(CreateEvent(NULL, TRUE, FALSE, NULL)),
		mkdirFinished(CreateEvent(NULL, TRUE, FALSE, NULL)),
		mkdirRelease(CreateEvent(NULL, TRUE, TRUE, NULL)),
		abortCount(0),
		sendCallCount(0),
		prioritySequence(0),
		waitingSaveCount(0),
		waitingTailCount(0),
		waitingSaveOrder(0),
		waitingTailOrder(0),
		activeSaveCount(0),
		activeFollowUpOrder(0),
		activeProbeCount(0) {
	}

	~FakeWrapperState() {
		CloseHandle(sendStarted);
		CloseHandle(sendRelease);
		CloseHandle(firstSendStarted);
		CloseHandle(firstSendRelease);
		CloseHandle(secondSendStarted);
		CloseHandle(secondSendRelease);
		CloseHandle(waitingBlockStarted);
		CloseHandle(waitingBlockRelease);
		CloseHandle(activeSaveStarted);
		CloseHandle(activeSaveRelease);
		CloseHandle(activeProbeStarted);
		CloseHandle(activeProbeRelease);
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
	HANDLE waitingBlockStarted;
	HANDLE waitingBlockRelease;
	HANDLE activeSaveStarted;
	HANDLE activeSaveRelease;
	HANDLE activeProbeStarted;
	HANDLE activeProbeRelease;
	HANDLE mkdirStarted;
	HANDLE mkdirFinished;
	HANDLE mkdirRelease;
	volatile LONG abortCount;
	volatile LONG sendCallCount;
	volatile LONG prioritySequence;
	volatile LONG waitingSaveCount;
	volatile LONG waitingTailCount;
	volatile LONG waitingSaveOrder;
	volatile LONG waitingTailOrder;
	volatile LONG activeSaveCount;
	volatile LONG activeFollowUpOrder;
	volatile LONG activeProbeCount;
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
	HANDLE WaitingBlockStarted() const { return m_state->waitingBlockStarted; }
	HANDLE ActiveSaveStarted() const { return m_state->activeSaveStarted; }
	HANDLE ActiveProbeStarted() const { return m_state->activeProbeStarted; }
	void ReleaseFirstSend() { SetEvent(m_state->firstSendRelease); }
	void ReleaseSecondSend() { SetEvent(m_state->secondSendRelease); }
	void ReleaseWaitingBlock() { SetEvent(m_state->waitingBlockRelease); }
	void ReleaseActiveSave() { SetEvent(m_state->activeSaveRelease); }
	void ReleaseActiveProbe() { SetEvent(m_state->activeProbeRelease); }
	int AbortCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->abortCount, 0, 0)); }
	int SendCallCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->sendCallCount, 0, 0)); }
	int WaitingSaveCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->waitingSaveCount, 0, 0)); }
	int WaitingTailCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->waitingTailCount, 0, 0)); }
	int WaitingSaveOrder() const { return static_cast<int>(InterlockedCompareExchange(&m_state->waitingSaveOrder, 0, 0)); }
	int WaitingTailOrder() const { return static_cast<int>(InterlockedCompareExchange(&m_state->waitingTailOrder, 0, 0)); }
	int ActiveSaveCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->activeSaveCount, 0, 0)); }
	int ActiveFollowUpOrder() const { return static_cast<int>(InterlockedCompareExchange(&m_state->activeFollowUpOrder, 0, 0)); }
	int ActiveProbeCount() const { return static_cast<int>(InterlockedCompareExchange(&m_state->activeProbeCount, 0, 0)); }

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
		if (remotePath && strstr(remotePath, "waiting-block.txt")) {
			SetEvent(m_state->waitingBlockStarted);
			WaitForSingleObject(m_state->waitingBlockRelease, INFINITE);
			return 0;
		}
		if (remotePath && strstr(remotePath, "waiting-save.txt")) {
			InterlockedIncrement(&m_state->waitingSaveCount);
			InterlockedExchange(&m_state->waitingSaveOrder, InterlockedIncrement(&m_state->prioritySequence));
			return 0;
		}
		if (remotePath && strstr(remotePath, "waiting-tail.txt")) {
			InterlockedIncrement(&m_state->waitingTailCount);
			InterlockedExchange(&m_state->waitingTailOrder, InterlockedIncrement(&m_state->prioritySequence));
			return 0;
		}
		if (remotePath && strstr(remotePath, "active-save.txt")) {
			LONG call = InterlockedIncrement(&m_state->activeSaveCount);
			if (call == 1) {
				SetEvent(m_state->activeSaveStarted);
				WaitForSingleObject(m_state->activeSaveRelease, INFINITE);
			} else {
				InterlockedExchange(&m_state->activeFollowUpOrder, InterlockedIncrement(&m_state->prioritySequence));
			}
			return 0;
		}
		if (remotePath && strstr(remotePath, "active-probe.txt")) {
			InterlockedIncrement(&m_state->activeProbeCount);
			SetEvent(m_state->activeProbeStarted);
			WaitForSingleObject(m_state->activeProbeRelease, INFINITE);
			return 0;
		}
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
		SetEvent(m_state->waitingBlockRelease);
		SetEvent(m_state->activeSaveRelease);
		SetEvent(m_state->activeProbeRelease);
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
		if (!BeginPerform())
			return m_result;
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
		if (!BeginPerform())
			return m_result;
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

class HandoffBlockingUpload : public QueueRemoteUploadFile {
public:
	HandoffBlockingUpload(HWND hwnd, RemoteUploadBatch * batch) :
		QueueRemoteUploadFile(hwnd, "/site/local.txt", TEXT("local.txt"), Mode_Binary, batch),
		m_handoffReached(CreateEvent(NULL, TRUE, FALSE, NULL)),
		m_release(CreateEvent(NULL, TRUE, FALSE, NULL)) {
	}

	~HandoffBlockingUpload() {
		CloseHandle(m_handoffReached);
		CloseHandle(m_release);
	}

	virtual void OnExecutionHandoff() {
		SetEvent(m_handoffReached);
		WaitForSingleObject(m_release, INFINITE);
	}

	HANDLE HandoffReached() const { return m_handoffReached; }
	void ReleasePerform() { SetEvent(m_release); }

private:
	HANDLE m_handoffReached;
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
		if (operation->GetType() == QueueOperation::QueueTypeUpload && operation->GetNotifyData()) {
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

static bool PumpUntilSchedulerSize(ConcurrentUploadScheduler * scheduler, int expected, DWORD timeoutMs)
{
	DWORD started = GetTickCount();
	while (scheduler->GetQueueSize() != expected) {
		PumpMessages();
		if (GetTickCount() - started >= timeoutMs)
			return false;
		Sleep(1);
	}
	PumpMessages();
	return true;
}

static bool PumpUntilSchedulerState(ConcurrentUploadScheduler * scheduler, int expectedSize, int expectedActive, DWORD timeoutMs)
{
	DWORD started = GetTickCount();
	while (scheduler->GetQueueSize() != expectedSize || scheduler->GetActiveCount() != expectedActive) {
		PumpMessages();
		if (GetTickCount() - started >= timeoutMs)
			return false;
		Sleep(1);
	}
	PumpMessages();
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

static bool TestPostStartPrePerformTeardown(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper wrapper;
	FTPQueue queue(&wrapper);
	harness->markerQueue = NULL;
	if (!Check(queue.Initialize() == 0, "initialize handoff teardown queue")) return false;

	RemoteUploadBatch * batch = MakeFileBatch(hwnd, harness->generation);
	HandoffBlockingUpload * upload = new HandoffBlockingUpload(hwnd, batch);
	LONG postsBefore = harness->completionPosts;
	LONG dropsBefore = harness->droppedPosts;
	queue.AddQueueOp(upload);
	if (!Check(PumpUntilEvent(upload->HandoffReached(), 5000),
		"upload reaches post-StartExecution pre-Perform handoff")) return false;

	InterlockedIncrement(&harness->generation);
	if (!Check(queue.BeginTeardown() == 0, "teardown wins upload perform handoff")) return false;
	upload->ReleasePerform();
	queue.Deinitialize();
	if (!Check(PumpUntil(&harness->completionPosts, postsBefore + 1, 5000),
		"handoff teardown posts one terminal completion")) return false;
	PumpFor(100);
	if (!Check(wrapper.SendCallCount() == 0 && wrapper.AbortCount() == 0,
		"handoff teardown needs neither SendFile nor wrapper Abort")) return false;
	if (!Check(batch->GetCanceledFileCount() == 1 && batch->GetRemainingFileCount() == 0 &&
		harness->completionPosts == postsBefore + 1 && harness->droppedPosts == dropsBefore + 1,
		"handoff teardown terminal-accounts exactly once")) return false;
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

static bool TestWaitingSavePromotion(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper prototype;
	ConcurrentUploadScheduler scheduler(hwnd, &prototype, 1, NULL);
	harness->markerQueue = NULL;
	if (!Check(scheduler.Initialize() == 0, "initialize waiting-save scheduler")) return false;

	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/waiting-block.txt", TEXT("waiting-block.txt"), Mode_Binary),
		UploadPriorityNormal) == 0, "queue active normal upload")) return false;
	if (!Check(PumpUntilEvent(prototype.WaitingBlockStarted(), 5000), "active normal upload enters SendFile")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/waiting-tail.txt", TEXT("waiting-tail.txt"), Mode_Binary),
		UploadPriorityNormal) == 0, "queue earlier normal tail")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/waiting-save.txt", TEXT("waiting-save.txt"), Mode_Binary),
		UploadPriorityNormal) == 0, "queue waiting manual save target")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/waiting-save.txt", TEXT("waiting-save.txt"), Mode_Binary),
		UploadPriorityUrgent) == 0, "promote waiting manual upload")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/waiting-save.txt", TEXT("waiting-save.txt"), Mode_Binary),
		UploadPriorityUrgent) == 0, "dedupe repeated waiting save")) return false;
	if (!Check(scheduler.GetQueueSize() == 3 && scheduler.GetActiveCount() == 1,
		"waiting promotion keeps one active and two waiting uploads")) return false;
	if (!Check(prototype.WaitingSaveCount() == 0 && prototype.WaitingTailCount() == 0 && prototype.AbortCount() == 0,
		"urgent save does not interrupt active normal upload")) return false;

	prototype.ReleaseWaitingBlock();
	if (!Check(PumpUntilSchedulerSize(&scheduler, 0, 5000), "waiting promotion scheduler drains")) return false;
	if (!Check(prototype.WaitingSaveCount() == 1 && prototype.WaitingTailCount() == 1,
		"waiting save promotion does not duplicate transfer")) return false;
	if (!Check(prototype.WaitingSaveOrder() > 0 && prototype.WaitingSaveOrder() < prototype.WaitingTailOrder(),
		"promoted save runs before earlier normal tail")) return false;
	if (!Check(prototype.AbortCount() == 0, "waiting promotion never aborts active work")) return false;

	scheduler.Deinitialize();
	return true;
}

static bool TestActiveSaveUrgentFollowUp(HWND hwnd, WindowHarness * harness)
{
	FakeWrapper prototype;
	ConcurrentUploadScheduler scheduler(hwnd, &prototype, 2, NULL);
	harness->markerQueue = NULL;
	if (!Check(scheduler.Initialize() == 0, "initialize active-save scheduler")) return false;

	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/active-save.txt", TEXT("active-save.txt"), Mode_Binary),
		UploadPriorityNormal) == 0, "queue active manual upload")) return false;
	if (!Check(PumpUntilEvent(prototype.ActiveSaveStarted(), 5000), "active manual upload enters SendFile")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/active-save.txt", TEXT("active-save.txt"), Mode_Binary),
		UploadPriorityUrgent) == 0, "queue urgent follow-up behind active upload")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/active-save.txt", TEXT("active-save.txt"), Mode_Binary),
		UploadPriorityUrgent) == 0, "dedupe repeated active save")) return false;
	if (!Check(scheduler.AddQueueOp(new QueueUpload(hwnd, "/site/active-probe.txt", TEXT("active-probe.txt"), Mode_Binary),
		UploadPriorityNormal) == 0, "queue non-conflicting scheduler probe")) return false;
	if (!Check(PumpUntilEvent(prototype.ActiveProbeStarted(), 5000),
		"idle worker starts non-conflicting probe after examining blocked urgent follow-up")) return false;
	if (!Check(scheduler.GetQueueSize() == 3 && scheduler.GetActiveCount() == 2,
		"scheduler keeps conflicting urgent follow-up waiting while probe is active")) return false;
	if (!Check(prototype.ActiveSaveCount() == 1 && prototype.ActiveFollowUpOrder() == 0 &&
		prototype.ActiveProbeCount() == 1 && prototype.AbortCount() == 0,
		"matching urgent follow-up neither overlaps nor interrupts active upload")) return false;

	prototype.ReleaseActiveProbe();
	if (!Check(PumpUntilSchedulerState(&scheduler, 2, 1, 5000),
		"probe completion leaves active upload and one urgent follow-up")) return false;
	prototype.ReleaseActiveSave();
	if (!Check(PumpUntilSchedulerSize(&scheduler, 0, 5000), "active follow-up scheduler drains")) return false;
	if (!Check(prototype.ActiveSaveCount() == 2 && prototype.ActiveFollowUpOrder() > 0,
		"active upload receives one final urgent follow-up")) return false;
	if (!Check(prototype.AbortCount() == 0, "active urgent follow-up never aborts active work")) return false;

	scheduler.Deinitialize();
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
	if (!TestPostStartPrePerformTeardown(hwnd, &harness)) return 1;
	if (!TestWaitingSavePromotion(hwnd, &harness)) return 1;
	if (!TestActiveSaveUrgentFollowUp(hwnd, &harness)) return 1;
	if (!TestSchedulerOppositeCompletionOrder(hwnd, &harness)) return 1;
	if (!TestFinishedPrepareTeardown(hwnd, &harness)) return 1;
	if (!TestActiveCompletionMarkerTeardown(hwnd, &harness)) return 1;
	if (!TestRejectionAndZeroSelected(hwnd, &harness)) return 1;
	DestroyWindow(hwnd);
	printf("ftp_queue_terminal_lifecycle_exit=0\n");
	return 0;
}
