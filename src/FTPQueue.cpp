/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "StdInc.h"
#include "FTPQueue.h"
#include "QueueTerminalLifecycle.h"
#include "file_size_utils.h"

#include <memory>

const int ConditionQueueOps = 0;
const int ConditionQueueStop = 1;
const int ConditionQueueAcked = 2;
const int ConditionCount = 3;

const LONG QueueExecutionIdle = 0;
const LONG QueueExecutionRunning = 1;
const LONG QueueExecutionFinished = 2;
const LONG QueueExecutionCanceled = 3;

FTPQueue::FTPQueue(FTPClientWrapper* wrapper, FTPQueueTerminalCallback terminalCallback, void * terminalContext) :
	m_wrapper(wrapper),
	m_running(false),
	m_stopping(false),
	m_performing(false),
	m_executionState(QueueExecutionIdle),
	m_teardown(false),
	m_activeOp(NULL),
	m_activeTerminalOp(NULL),
	m_threadHandle(NULL),
	m_terminalCallback(terminalCallback),
	m_terminalContext(terminalContext)
{
	m_monitor = new Monitor(ConditionCount);
	m_wrapper->SetProgressMonitor(this);
}

FTPQueue::~FTPQueue() {
	delete m_monitor;
	if (m_running)
		OutErr("[Queue] Error: queue still running\n");
}

int FTPQueue::Initialize() {
	if (m_running)
		return 0;

	m_stopping = false;
	m_teardown = false;
	m_running = true;

	m_threadHandle = ::CreateThread(NULL, 0, &ThreadProc, this, 0, NULL);
	if (!m_threadHandle) {
		m_running = false;
		return -1;
	}

	return 0;
}

int FTPQueue::Deinitialize() {
	if (!m_running)
		return 0;

	//Notifications sent from this function are handled immediatly, since they're from the local thread.
	//Therefore, peekmessage won't remove any messages that this function sent, as they won;t be queued

	m_monitor->Enter();
		m_stopping = true;

		if (m_performing) {
			m_queue.front()->Terminate();
			m_queue.front()->SendNotification(QueueOperation::QueueEventEnd);
			//m_queue.front()->SendNotification(QueueOperation::QueueEventRemove);
		}
		for (VQueue::iterator it = m_queue.begin(); it != m_queue.end(); ++it)
			(*it)->Terminate();

		m_monitor->Signal(ConditionQueueOps);
		m_monitor->Wait(ConditionQueueStop);
	m_monitor->Exit();

	HANDLE threadHandle = m_threadHandle;
	m_threadHandle = NULL;
	if (threadHandle) {
		::WaitForSingleObject(threadHandle, INFINITE);
		::CloseHandle(threadHandle);
	}

	while (!m_queue.empty()) {
		QueueOperation * op = m_queue.front();
		QueueOperation * terminalOp = NULL;
		//Remove any remaining messages (most notably Progress messages)
		if (op != m_activeOp)
			terminalOp = queue_cancel_and_take_terminal(op);
		else {
			terminalOp = m_activeTerminalOp;
			m_activeTerminalOp = NULL;
		}
		op->ClearPendingNotifications();
		op->SendNotification(QueueOperation::QueueEventRemove);
		m_queue.pop_front();
		if (m_terminalCallback)
			m_terminalCallback(m_terminalContext, op, terminalOp);
		else
			delete terminalOp;
		delete op;
	}

	m_running = false;
	m_stopping = false;
	m_performing = false;
	InterlockedExchange(&m_executionState, QueueExecutionIdle);
	m_teardown = false;
	m_activeOp = NULL;
	m_activeTerminalOp = NULL;

	return 0;
}

int FTPQueue::AddQueueOp(QueueOperation * op, bool sendAddNotification) {
	if (!op)
		return -1;
	std::unique_ptr<QueueOperation> owned(op);
	op->SetClient(m_wrapper);

	m_monitor->Enter();
		if (m_teardown || m_stopping) {
			m_monitor->Exit();
			delete queue_cancel_and_take_terminal(op);
			return -1;
		}
		VQueue::iterator it;
		for(it = m_queue.begin(); it != m_queue.end(); ++it) {
			if (op->Equals(**it)) {
				OutMsg("[Queue] The operation was already added to the queue, ignoring");
				m_monitor->Exit();
				return 0;
			}
		}
	m_monitor->Exit();

	//Can safely inform queueWindow, Add is called by window thread
	if (sendAddNotification)
		op->SendNotification(QueueOperation::QueueEventAdd);

	m_monitor->Enter();
		m_queue.push_back(owned.release());
		if (m_queue.size() == 1)
			m_monitor->Signal(ConditionQueueOps);
	m_monitor->Exit();

	return 0;
}

int FTPQueue::GetQueueSize() const {
	int res = 0;

	m_monitor->Enter();
	res = m_queue.size();
	m_monitor->Exit();

	return res;
}

int FTPQueue::ClearQueue(bool suppressTerminalFollowUps) {
	QueueOperation * op = NULL;
	VQueue completionMarkers;
	if (suppressTerminalFollowUps)
		BeginTeardown();

	m_monitor->Enter();
		if (m_performing) {
			op = m_queue.front();
			m_queue.pop_front();
		}
		while (!m_queue.empty()) {
			QueueOperation * pending = m_queue.front();
			m_queue.pop_front();
			bool isCompletionMarker = pending->GetType() == QueueOperation::QueueTypeRemoteUploadComplete ||
				pending->GetType() == QueueOperation::QueueTypeRemoteDownloadComplete;
			// Outside session teardown, preserve completion markers after any active transfer.
			if (isCompletionMarker && !suppressTerminalFollowUps) {
				completionMarkers.push_back(pending);
				continue;
			}
			QueueOperation * terminalOp = isCompletionMarker ? NULL : queue_cancel_and_take_terminal(pending);
			terminalOp = queue_filter_terminal_follow_up(terminalOp, suppressTerminalFollowUps);
			pending->SendNotification(QueueOperation::QueueEventRemove);
			delete pending;
			if (terminalOp) {
				terminalOp->SetClient(m_wrapper);
				terminalOp->SendNotification(QueueOperation::QueueEventAdd);
				completionMarkers.push_back(terminalOp);
			}
		}
		if (m_performing) {
			m_queue.push_back(op);
		}
		while (!completionMarkers.empty()) {
			m_queue.push_back(completionMarkers.front());
			completionMarkers.pop_front();
		}
		if (!m_performing && !m_queue.empty())
			m_monitor->Signal(ConditionQueueOps);
	m_monitor->Exit();

	return 0;
}

int FTPQueue::CancelQueueOp(QueueOperation * op, QueueOperation ** terminalOp, bool notifyTerminalCallback) {
	QueueOperation * canceled = NULL;
	if (terminalOp)
		*terminalOp = NULL;

	m_monitor->Enter();
		if (m_performing) {
			if (op == m_queue.front()) {
				m_monitor->Exit();
				return -1;		//Cannot cancel running operation, only abort
			}
		}

		for(VQueue::iterator it = m_queue.begin(); it != m_queue.end(); ++it) {
			if (*it == op) {
				canceled = *it;
				m_queue.erase(it);
				break;
			}
		}
	m_monitor->Exit();

	if (!canceled)
		return 1;

	canceled->OnQueueCanceled();
	canceled->SendNotification(QueueOperation::QueueEventRemove);
	QueueOperation * followUp = canceled->OnQueueTerminal();
	if (notifyTerminalCallback && m_terminalCallback)
		m_terminalCallback(m_terminalContext, canceled, followUp);
	delete canceled;

	if (terminalOp) {
		*terminalOp = followUp;
	} else if (followUp && (!notifyTerminalCallback || !m_terminalCallback)) {
		AddQueueOp(followUp);
	}
	return 0;
}

int FTPQueue::BeginTeardown() {
	bool abortActive = false;

	m_monitor->Enter();
		if (!m_teardown) {
			m_teardown = true;
			if (m_performing && m_activeOp) {
				LONG state = InterlockedCompareExchange(&m_executionState, QueueExecutionCanceled, QueueExecutionRunning);
				if (state == QueueExecutionIdle)
					state = InterlockedCompareExchange(&m_executionState, QueueExecutionCanceled, QueueExecutionIdle);
				if (state == QueueExecutionIdle || state == QueueExecutionRunning)
					m_activeOp->OnQueueCanceled();
				abortActive = state == QueueExecutionRunning;
				m_activeOp->Terminate();
				m_activeOp->ClearPendingNotifications();
			}
		}
	m_monitor->Exit();

	return abortActive ? m_wrapper->Abort() : 0;
}

int FTPQueue::AbortActive() {
	bool active = false;
	m_monitor->Enter();
		if (m_performing && m_activeOp &&
			InterlockedCompareExchange(&m_executionState, QueueExecutionCanceled, QueueExecutionRunning) == QueueExecutionRunning) {
			m_activeOp->OnQueueCanceled();
			active = true;
		}
	m_monitor->Exit();
	return active ? m_wrapper->Abort() : 0;
}

int FTPQueue::QueueLoop() {
	QueueOperation * op = NULL;

	while(!m_stopping) {

		m_monitor->Enter();
			while (m_queue.empty() && !m_stopping)
				m_monitor->Wait(ConditionQueueOps);

			if (m_stopping) {
				m_monitor->Exit();
				break;
			}

			op = m_queue.front();
			m_activeOp = op;
			m_performing = true;
			InterlockedExchange(&m_executionState, QueueExecutionIdle);
		m_monitor->Exit();

		op->SendNotification(QueueOperation::QueueEventStart);
		op->SetRunning(true);
		Sleep(500);
		if (InterlockedCompareExchange(&m_executionState, QueueExecutionRunning, QueueExecutionIdle) == QueueExecutionIdle) {
			op->Perform();
			InterlockedCompareExchange(&m_executionState, QueueExecutionFinished, QueueExecutionRunning);
		} else {
			op->OnQueueCanceled();
		}
		op->SetRunning(false);
		QueueOperation * terminalOp = queue_end_and_take_terminal(op, QueueOperation::QueueEventEnd);

		m_monitor->Enter();
			if (terminalOp && !m_stopping) {
				terminalOp->SetClient(m_wrapper);
				m_queue.push_back(terminalOp);
			} else if (terminalOp)
				m_activeTerminalOp = terminalOp;
		m_monitor->Exit();

		if (terminalOp && !m_stopping)
			terminalOp->SendNotification(QueueOperation::QueueEventAdd);

		if (m_stopping)
			break;

		op->SendNotification(QueueOperation::QueueEventRemove);

		m_monitor->Enter();
			m_activeOp = NULL;
			m_performing = false;
			InterlockedExchange(&m_executionState, QueueExecutionIdle);
			m_queue.pop_front();
		m_monitor->Exit();

		if (m_terminalCallback)
			m_terminalCallback(m_terminalContext, op, NULL);
		delete op;
	}

	m_monitor->Enter();
		m_performing = false;
		InterlockedExchange(&m_executionState, QueueExecutionIdle);
		m_monitor->Signal(ConditionQueueStop);
	m_monitor->Exit();

	return 0;
}

int FTPQueue::OnDataReceived(LONGLONG received, LONGLONG total) {
	m_monitor->Enter();
		if (!m_performing || !m_activeOp) {	//not called from performing thread?
			m_monitor->Exit();
			return -1;
		}
	m_monitor->Exit();


	if (total <= 0) {
		m_activeOp->SetProgress(-1.0f);
	} else {
		m_activeOp->SetProgress(file_size_progress_percent(received, total));
	}
	m_activeOp->SendNotification(QueueOperation::QueueEventProgress);
	return 0;
}

int FTPQueue::OnDataSent(LONGLONG sent, LONGLONG total) {
	m_monitor->Enter();
		if (!m_performing || !m_activeOp) {	//not called from performing thread?
			m_monitor->Exit();
			return -1;
		}
	m_monitor->Exit();


	if (total <= 0) {
		m_activeOp->SetProgress(-1.0f);
	} else {
		m_activeOp->SetProgress(file_size_progress_percent(sent, total));
	}
	m_activeOp->SendNotification(QueueOperation::QueueEventProgress);
	return 0;
}

int FTPQueue::QueueThread(FTPQueue* queue) {
	return queue->QueueLoop();
}

DWORD WINAPI ThreadProc(LPVOID param) {
	FTPQueue* queue = (FTPQueue*)param;
	return FTPQueue::QueueThread(queue);
}
