/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#ifndef CONCURRENTUPLOADSCHEDULER_H
#define CONCURRENTUPLOADSCHEDULER_H

#include <windows.h>
#include <deque>
#include <vector>

#include "UploadSchedulingPolicy.h"

class FTPClientWrapper;
class FTPQueue;
class Monitor;
class QueueOperation;

class ConcurrentUploadScheduler {
public:
	ConcurrentUploadScheduler(HWND hNotify, FTPClientWrapper * prototype, int workerCount);
	~ConcurrentUploadScheduler();

	int Initialize();
	int Deinitialize();
	int AddQueueOp(QueueOperation * op, UploadPriority priority);
	int CancelQueueOp(QueueOperation * op);
	int AbortActive();
	int GetQueueSize() const;
	int GetActiveCount() const;

private:
	struct Worker;
	struct PendingAdd {
		PendingAdd(QueueOperation * queueOperation, UploadPriority queuePriority) :
			operation(queueOperation),
			priority(queuePriority),
			canceled(false) {
		}

		QueueOperation * operation;
		UploadPriority priority;
		bool canceled;
	};

	int SchedulerLoop();
	Worker * FindIdleWorkerLocked();
	bool FindWorkerConflictLocked(const QueueOperation & op) const;
	QueueOperation * FindWaitingDuplicateLocked(QueueOperation & op) const;
	PendingAdd * FindPendingDuplicateLocked(QueueOperation & op);
	PendingAdd * FindPendingAddLocked(QueueOperation * op);
	void DeletePendingOperations();
	void DeleteWorkers();
	static void DeleteWorkerList(std::vector<Worker*> & workers);
	static void WorkerOperationTerminal(void * context, QueueOperation * op);

	static DWORD WINAPI SchedulerThread(LPVOID param);

	FTPClientWrapper * m_prototype;
	Monitor * m_monitor;
	UploadSchedulingPolicy m_policy;
	std::deque<PendingAdd> m_pendingAdds;
	std::vector<Worker*> m_workers;
	HANDLE m_dispatchThread;
	int m_workerCount;
	bool m_running;
	bool m_initializing;
	bool m_stopping;
	bool m_accepting;
};

#endif //CONCURRENTUPLOADSCHEDULER_H
