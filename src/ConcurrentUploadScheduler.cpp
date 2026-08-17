/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#include "StdInc.h"
#include "ConcurrentUploadScheduler.h"

#include "FTPQueue.h"
#include "Monitor.h"
#include "QueueOperation.h"

struct ConcurrentUploadScheduler::Worker {
	Worker(FTPClientWrapper * workerWrapper) :
		wrapper(workerWrapper),
		queue(new FTPQueue(workerWrapper)),
		operation(NULL) {
	}

	FTPClientWrapper * wrapper;
	FTPQueue * queue;
	QueueOperation * operation;
};

ConcurrentUploadScheduler::ConcurrentUploadScheduler(HWND hNotify, FTPClientWrapper * prototype, int workerCount) :
	m_hNotify(hNotify),
	m_prototype(prototype),
	m_monitor(new Monitor(1)),
	m_dispatchThread(NULL),
	m_workerCount(workerCount),
	m_running(false),
	m_stopping(false),
	m_accepting(false) {
	if (m_workerCount < 1)
		m_workerCount = 1;
	if (m_workerCount > 8)
		m_workerCount = 8;
}

ConcurrentUploadScheduler::~ConcurrentUploadScheduler() {
	Deinitialize();
	delete m_monitor;
}

int ConcurrentUploadScheduler::Initialize() {
	m_monitor->Enter();
	if (m_running || !m_prototype) {
		m_monitor->Exit();
		return m_running ? 0 : -1;
	}
	m_monitor->Exit();

	std::vector<Worker*> workers;
	for (int i = 0; i < m_workerCount; ++i) {
		FTPClientWrapper * clone = m_prototype->Clone();
		if (!clone) {
			for (std::vector<Worker*>::iterator it = workers.begin(); it != workers.end(); ++it) {
				(*it)->queue->Deinitialize();
				delete (*it)->queue;
				delete (*it)->wrapper;
				delete *it;
			}
			return -1;
		}
		Worker * worker = new Worker(clone);
		if (worker->queue->Initialize() != 0) {
			delete worker->queue;
			delete worker->wrapper;
			delete worker;
			for (std::vector<Worker*>::iterator it = workers.begin(); it != workers.end(); ++it) {
				(*it)->queue->Deinitialize();
				delete (*it)->queue;
				delete (*it)->wrapper;
				delete *it;
			}
			return -1;
		}
		workers.push_back(worker);
	}

	m_monitor->Enter();
	m_workers.swap(workers);
	m_stopping = false;
	m_accepting = true;
	m_running = true;
	m_dispatchThread = ::CreateThread(NULL, 0, &ConcurrentUploadScheduler::SchedulerThread, this, 0, NULL);
	if (!m_dispatchThread) {
		m_running = false;
		m_accepting = false;
		m_stopping = true;
		m_monitor->Exit();
		DeleteWorkers();
		return -1;
	}
	m_monitor->Exit();

	return 0;
}

int ConcurrentUploadScheduler::Deinitialize() {
	m_monitor->Enter();
	if (!m_running) {
		m_monitor->Exit();
		return 0;
	}
	m_accepting = false;
	m_stopping = true;
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->operation)
			(*it)->operation->Terminate();
		(*it)->wrapper->Abort();
	}
	HANDLE dispatchThread = m_dispatchThread;
	m_monitor->Exit();

	if (dispatchThread) {
		::WaitForSingleObject(dispatchThread, INFINITE);
		::CloseHandle(dispatchThread);
	}

	m_monitor->Enter();
	m_dispatchThread = NULL;
	m_monitor->Exit();

	DeleteWorkers();
	DeletePendingOperations();

	m_monitor->Enter();
	m_running = false;
	m_stopping = false;
	m_monitor->Exit();
	return 0;
}

int ConcurrentUploadScheduler::AddQueueOp(QueueOperation * op, UploadPriority priority) {
	if (!op)
		return -1;

	m_monitor->Enter();
	if (!m_accepting) {
		m_monitor->Exit();
		delete op;
		return -1;
	}
	QueueOperation * duplicate = FindWaitingDuplicateLocked(*op);
	if (duplicate) {
		if (priority == UploadPriorityUrgent)
			m_policy.PromoteWaiting(duplicate);
		m_monitor->Exit();
		delete op;
		return 0;
	}
	m_monitor->Exit();

	// Keep the established UI lifecycle even while this operation waits for a worker.
	op->SendNotification(QueueOperation::QueueEventAdd);

	m_monitor->Enter();
	if (!m_accepting) {
		m_monitor->Exit();
		op->OnQueueCanceled();
		op->SendNotification(QueueOperation::QueueEventRemove);
		delete op;
		return -1;
	}
	m_policy.Push(op, priority);
	m_monitor->Exit();

	return 0;
}

int ConcurrentUploadScheduler::CancelQueueOp(QueueOperation * op) {
	if (!op)
		return -1;

	m_monitor->Enter();
	if (m_policy.Remove(op)) {
		m_monitor->Exit();
		op->OnQueueCanceled();
		op->SendNotification(QueueOperation::QueueEventRemove);
		delete op;
		return 0;
	}
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->operation == op) {
			m_monitor->Exit();
			return -1;
		}
	}
	m_monitor->Exit();
	return 0;
}

int ConcurrentUploadScheduler::AbortActive() {
	m_monitor->Enter();
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->operation)
			(*it)->wrapper->Abort();
	}
	m_monitor->Exit();
	return 0;
}

int ConcurrentUploadScheduler::GetQueueSize() const {
	int size = 0;
	m_monitor->Enter();
	size = static_cast<int>(m_policy.GetQueueSize());
	for (std::vector<Worker*>::const_iterator it = m_workers.begin(); it != m_workers.end(); ++it)
		size += (*it)->queue->GetQueueSize();
	m_monitor->Exit();
	return size;
}

int ConcurrentUploadScheduler::GetActiveCount() const {
	int count = 0;
	m_monitor->Enter();
	for (std::vector<Worker*>::const_iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->operation && (*it)->queue->GetQueueSize() > 0)
			++count;
	}
	m_monitor->Exit();
	return count;
}

int ConcurrentUploadScheduler::SchedulerLoop() {
	while (true) {
		Worker * worker = NULL;
		QueueOperation * op = NULL;

		m_monitor->Enter();
		ReclaimFinishedWorkersLocked();
		if (m_stopping) {
			m_monitor->Exit();
			break;
		}
		worker = FindIdleWorkerLocked();
		if (worker)
			op = m_policy.TakeNext();
		if (op)
			worker->operation = op;
		m_monitor->Exit();

		if (!op) {
			::Sleep(25);
			continue;
		}

		// FTPQueue binds the operation to this clone immediately before it can run.
		worker->queue->AddQueueOp(op);
	}
	return 0;
}

int ConcurrentUploadScheduler::ReclaimFinishedWorkersLocked() {
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->operation && (*it)->queue->GetQueueSize() == 0)
			(*it)->operation = NULL;
	}
	return 0;
}

ConcurrentUploadScheduler::Worker * ConcurrentUploadScheduler::FindIdleWorkerLocked() {
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if (!(*it)->operation && (*it)->queue->GetQueueSize() == 0)
			return *it;
	}
	return NULL;
}

QueueOperation * ConcurrentUploadScheduler::FindWaitingDuplicateLocked(QueueOperation & op) const {
	return m_policy.FindWaiting([&op](QueueOperation * waiting) {
		return op.Equals(*waiting);
	});
}

void ConcurrentUploadScheduler::DeletePendingOperations() {
	while (true) {
		m_monitor->Enter();
		QueueOperation * op = m_policy.TakeNext();
		m_monitor->Exit();
		if (!op)
			break;
		op->OnQueueCanceled();
		op->ClearPendingNotifications();
		op->SendNotification(QueueOperation::QueueEventRemove);
		delete op;
	}
}

void ConcurrentUploadScheduler::DeleteWorkers() {
	m_monitor->Enter();
	std::vector<Worker*> workers;
	workers.swap(m_workers);
	m_monitor->Exit();

	for (std::vector<Worker*>::iterator it = workers.begin(); it != workers.end(); ++it) {
		(*it)->wrapper->Abort();
		(*it)->queue->Deinitialize();
		(*it)->wrapper->Disconnect();
		delete (*it)->queue;
		delete (*it)->wrapper;
		delete *it;
	}
}

DWORD WINAPI ConcurrentUploadScheduler::SchedulerThread(LPVOID param) {
	ConcurrentUploadScheduler * scheduler = static_cast<ConcurrentUploadScheduler*>(param);
	return static_cast<DWORD>(scheduler->SchedulerLoop());
}
