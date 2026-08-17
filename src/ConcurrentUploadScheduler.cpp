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
		queue(new FTPQueue(workerWrapper)) {
	}

	FTPClientWrapper * wrapper;
	FTPQueue * queue;
};

const int ConditionSchedulerState = 0;

ConcurrentUploadScheduler::ConcurrentUploadScheduler(HWND, FTPClientWrapper * prototype, int workerCount) :
	m_prototype(prototype),
	m_monitor(new Monitor(1)),
	m_dispatchThread(NULL),
	m_workerCount(workerCount),
	m_running(false),
	m_initializing(false),
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
	if (m_running || m_initializing || m_stopping || !m_prototype) {
		m_monitor->Exit();
		return (m_running && !m_stopping) ? 0 : -1;
	}
	m_initializing = true;

	std::vector<Worker*> workers;
	for (int i = 0; i < m_workerCount; ++i) {
		FTPClientWrapper * clone = m_prototype->Clone();
		if (!clone) {
			DeleteWorkerList(workers);
			m_initializing = false;
			m_monitor->Signal(ConditionSchedulerState);
			m_monitor->Exit();
			return -1;
		}
		Worker * worker = new Worker(clone);
		if (worker->queue->Initialize() != 0) {
			delete worker->queue;
			delete worker->wrapper;
			delete worker;
			DeleteWorkerList(workers);
			m_initializing = false;
			m_monitor->Signal(ConditionSchedulerState);
			m_monitor->Exit();
			return -1;
		}
		workers.push_back(worker);
	}

	m_workers.swap(workers);
	m_stopping = false;
	m_accepting = true;
	m_running = true;
	m_dispatchThread = ::CreateThread(NULL, 0, &ConcurrentUploadScheduler::SchedulerThread, this, 0, NULL);
	if (!m_dispatchThread) {
		m_workers.swap(workers);
		m_running = false;
		m_accepting = false;
		m_stopping = false;
		m_initializing = false;
		m_monitor->Signal(ConditionSchedulerState);
		m_monitor->Exit();
		DeleteWorkerList(workers);
		return -1;
	}
	m_initializing = false;
	m_monitor->Signal(ConditionSchedulerState);
	m_monitor->Exit();

	return 0;
}

int ConcurrentUploadScheduler::Deinitialize() {
	m_monitor->Enter();
	if (!m_running) {
		m_monitor->Exit();
		return 0;
	}
	if (m_stopping) {
		while (m_stopping)
			m_monitor->Wait(ConditionSchedulerState);
		m_monitor->Exit();
		return 0;
	}
	m_accepting = false;
	m_stopping = true;
	for (std::deque<PendingAdd>::iterator it = m_pendingAdds.begin(); it != m_pendingAdds.end(); ++it)
		it->operation->Terminate();
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
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
	while (!m_pendingAdds.empty())
		m_monitor->Wait(ConditionSchedulerState);
	m_monitor->Exit();

	DeleteWorkers();
	DeletePendingOperations();

	m_monitor->Enter();
	m_running = false;
	m_stopping = false;
	m_accepting = false;
	m_monitor->Signal(ConditionSchedulerState);
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
	PendingAdd * pendingDuplicate = NULL;
	if (!duplicate)
		pendingDuplicate = FindPendingDuplicateLocked(*op);
	if (duplicate || pendingDuplicate) {
		if (priority == UploadPriorityUrgent)
			if (duplicate)
				m_policy.PromoteWaiting(duplicate);
			else
				pendingDuplicate->priority = UploadPriorityUrgent;
		m_monitor->Exit();
		delete op;
		return 0;
	}
	m_pendingAdds.push_back(PendingAdd(op, priority));
	m_monitor->Exit();

	// Keep the operation out of the dispatch lanes until its Add notification is acknowledged.
	op->SendNotification(QueueOperation::QueueEventAdd);

	m_monitor->Enter();
	PendingAdd * pending = FindPendingAddLocked(op);
	bool canceled = !pending || pending->canceled || !m_accepting;
	UploadPriority pendingPriority = pending ? pending->priority : priority;
	if (pending) {
		for (std::deque<PendingAdd>::iterator it = m_pendingAdds.begin(); it != m_pendingAdds.end(); ++it) {
			if (it->operation == op) {
				m_pendingAdds.erase(it);
				break;
			}
		}
	}
	if (!canceled)
		m_policy.Push(op, pendingPriority);
	m_monitor->Signal(ConditionSchedulerState);
	m_monitor->Exit();

	if (!canceled)
		return 0;

	op->OnQueueCanceled();
	op->SendNotification(QueueOperation::QueueEventRemove);
	delete op;
	return -1;
}

int ConcurrentUploadScheduler::CancelQueueOp(QueueOperation * op) {
	if (!op)
		return -1;

	m_monitor->Enter();
	PendingAdd * pending = FindPendingAddLocked(op);
	if (pending) {
		pending->canceled = true;
		pending->operation->Terminate();
		m_monitor->Exit();
		return 0;
	}
	if (m_policy.Remove(op)) {
		m_monitor->Exit();
		op->OnQueueCanceled();
		op->SendNotification(QueueOperation::QueueEventRemove);
		delete op;
		return 0;
	}
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->queue->CancelQueueOp(op) == -1) {
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
		if ((*it)->queue->GetQueueSize() > 0)
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
		if ((*it)->queue->GetQueueSize() > 0)
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
		if (m_stopping) {
			m_monitor->Exit();
			break;
		}
		worker = FindIdleWorkerLocked();
		if (worker)
			op = m_policy.TakeNext();
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

ConcurrentUploadScheduler::Worker * ConcurrentUploadScheduler::FindIdleWorkerLocked() {
	for (std::vector<Worker*>::iterator it = m_workers.begin(); it != m_workers.end(); ++it) {
		if ((*it)->queue->GetQueueSize() == 0)
			return *it;
	}
	return NULL;
}

QueueOperation * ConcurrentUploadScheduler::FindWaitingDuplicateLocked(QueueOperation & op) const {
	return m_policy.FindWaiting([&op](QueueOperation * waiting) {
		return op.Equals(*waiting);
	});
}

ConcurrentUploadScheduler::PendingAdd * ConcurrentUploadScheduler::FindPendingDuplicateLocked(QueueOperation & op) {
	for (std::deque<PendingAdd>::iterator it = m_pendingAdds.begin(); it != m_pendingAdds.end(); ++it) {
		if (!it->canceled && op.Equals(*it->operation))
			return &*it;
	}
	return NULL;
}

ConcurrentUploadScheduler::PendingAdd * ConcurrentUploadScheduler::FindPendingAddLocked(QueueOperation * op) {
	for (std::deque<PendingAdd>::iterator it = m_pendingAdds.begin(); it != m_pendingAdds.end(); ++it) {
		if (it->operation == op)
			return &*it;
	}
	return NULL;
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
	DeleteWorkerList(workers);
}

void ConcurrentUploadScheduler::DeleteWorkerList(std::vector<Worker*> & workers) {
	for (std::vector<Worker*>::iterator it = workers.begin(); it != workers.end(); ++it) {
		(*it)->wrapper->Abort();
		(*it)->queue->Deinitialize();
		(*it)->wrapper->Disconnect();
		delete (*it)->queue;
		delete (*it)->wrapper;
		delete *it;
	}
	workers.clear();
}

DWORD WINAPI ConcurrentUploadScheduler::SchedulerThread(LPVOID param) {
	ConcurrentUploadScheduler * scheduler = static_cast<ConcurrentUploadScheduler*>(param);
	return static_cast<DWORD>(scheduler->SchedulerLoop());
}
