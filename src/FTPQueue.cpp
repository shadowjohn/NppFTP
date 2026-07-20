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
#include "file_size_utils.h"

#include <memory>

const int ConditionQueueOps = 0;
const int ConditionQueueStop = 1;
const int ConditionQueueAcked = 2;
const int ConditionCount = 3;

FTPQueue::FTPQueue(FTPClientWrapper* wrapper) :
	m_wrapper(wrapper),
	m_running(false),
	m_stopping(false),
	m_performing(false),
	m_activeOp(NULL)
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
	m_running = true;

	::CreateThread(NULL, 0, &ThreadProc, this, 0, NULL);

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

		m_monitor->Signal(ConditionQueueOps);
		m_monitor->Wait(ConditionQueueStop);
	m_monitor->Exit();

	while (!m_queue.empty()) {
		//Remove any remaining messages (most notably Progress messages)
		if (m_queue.front() != m_activeOp)
			m_queue.front()->OnQueueCanceled();
		m_queue.front()->ClearPendingNotifications();
		m_queue.front()->SendNotification(QueueOperation::QueueEventRemove);
		delete m_queue.front();
		m_queue.pop_front();
	}

	m_running = false;
	m_stopping = false;
	m_performing = false;

	return 0;
}

int FTPQueue::AddQueueOp(QueueOperation * op) {
	std::unique_ptr<QueueOperation> owned(op);
	op->SetClient(m_wrapper);

	m_monitor->Enter();
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

int FTPQueue::ClearQueue() {
	QueueOperation * op = NULL;
	VQueue completionMarkers;

	m_monitor->Enter();
		if (m_performing) {
			op = m_queue.front();
			m_queue.pop_front();
		}
		while (!m_queue.empty()) {
			QueueOperation * pending = m_queue.front();
			m_queue.pop_front();
			// Keep download completion markers so canceled file counts can be summarized after any active transfer.
			if (pending->GetType() == QueueOperation::QueueTypeRemoteDownloadComplete) {
				completionMarkers.push_back(pending);
				continue;
			}
			pending->OnQueueCanceled();
			pending->SendNotification(QueueOperation::QueueEventRemove);
			delete pending;
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

int FTPQueue::CancelQueueOp(QueueOperation * op) {
	m_monitor->Enter();
		if (m_performing) {
			if (op == m_queue.front()) {
				m_monitor->Exit();
				return -1;		//Cannot cancel running operation, only abort
			}
		}

		QueueOperation * queueop = NULL;
		for(VQueue::iterator it = m_queue.begin(); it != m_queue.end(); ++it) {
			queueop = *it;
			if (queueop == op) {
				m_queue.erase(it);
				queueop->OnQueueCanceled();
				queueop->SendNotification(QueueOperation::QueueEventRemove);
				delete queueop;
				break;
			}
		}
	m_monitor->Exit();

	return 0;
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
		m_monitor->Exit();

		op->SendNotification(QueueOperation::QueueEventStart);
		op->SetRunning(true);
		Sleep(500);
		op->Perform();
		op->SetRunning(false);
		op->SendNotification(QueueOperation::QueueEventEnd);

		if (m_stopping)
			break;

		op->SendNotification(QueueOperation::QueueEventRemove);

		m_monitor->Enter();
			m_activeOp = NULL;
			m_performing = false;
			m_queue.pop_front();
			delete op;
		m_monitor->Exit();
	}

	m_monitor->Enter();
		m_performing = false;
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
