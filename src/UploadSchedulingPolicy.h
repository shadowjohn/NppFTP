/*
    NppFTP: FTP/SFTP functionality for Notepad++
    Copyright (C) 2010  Harry (harrybharry@users.sourceforge.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#ifndef UPLOADSCHEDULINGPOLICY_H
#define UPLOADSCHEDULINGPOLICY_H

#include <deque>

class QueueOperation;

enum UploadPriority {
	UploadPriorityNormal,
	UploadPriorityUrgent
};

inline int upload_scheduling_queue_size(size_t waiting, size_t pendingAdds, int dispatched) {
	return static_cast<int>(waiting + pendingAdds) + dispatched;
}

class UploadSchedulingPolicy {
public:
	void Push(QueueOperation * op, UploadPriority priority) {
		if (priority == UploadPriorityUrgent)
			m_urgent.push_back(op);
		else
			m_normal.push_back(op);
	}

	QueueOperation * TakeNext() {
		if (!m_urgent.empty()) {
			QueueOperation * op = m_urgent.front();
			m_urgent.pop_front();
			return op;
		}
		if (!m_normal.empty()) {
			QueueOperation * op = m_normal.front();
			m_normal.pop_front();
			return op;
		}
		return NULL;
	}

	template <typename Predicate>
	QueueOperation * TakeNextMatching(Predicate predicate) {
		QueueOperation * op = TakeFirstMatching(m_urgent, predicate);
		if (op)
			return op;
		return TakeFirstMatching(m_normal, predicate);
	}

	bool PromoteWaiting(QueueOperation * op) {
		if (Contains(m_urgent, op))
			return true;

		for (std::deque<QueueOperation*>::iterator it = m_normal.begin(); it != m_normal.end(); ++it) {
			if (*it != op)
				continue;
			m_normal.erase(it);
			m_urgent.push_back(op);
			return true;
		}
		return false;
	}

	bool ContainsWaiting(QueueOperation * op) const {
		return Contains(m_urgent, op) || Contains(m_normal, op);
	}

	bool Remove(QueueOperation * op) {
		return RemoveFrom(m_urgent, op) || RemoveFrom(m_normal, op);
	}

	template <typename Predicate>
	QueueOperation * FindWaiting(Predicate predicate) const {
		for (std::deque<QueueOperation*>::const_iterator it = m_urgent.begin(); it != m_urgent.end(); ++it) {
			if (predicate(*it))
				return *it;
		}
		for (std::deque<QueueOperation*>::const_iterator it = m_normal.begin(); it != m_normal.end(); ++it) {
			if (predicate(*it))
				return *it;
		}
		return NULL;
	}

	size_t GetQueueSize() const {
		return m_urgent.size() + m_normal.size();
	}

private:
	template <typename Predicate>
	static QueueOperation * TakeFirstMatching(std::deque<QueueOperation*> & queue, Predicate predicate) {
		for (std::deque<QueueOperation*>::iterator it = queue.begin(); it != queue.end(); ++it) {
			if (!predicate(*it))
				continue;
			QueueOperation * op = *it;
			queue.erase(it);
			return op;
		}
		return NULL;
	}

	static bool Contains(const std::deque<QueueOperation*> & queue, QueueOperation * op) {
		for (std::deque<QueueOperation*>::const_iterator it = queue.begin(); it != queue.end(); ++it) {
			if (*it == op)
				return true;
		}
		return false;
	}

	static bool RemoveFrom(std::deque<QueueOperation*> & queue, QueueOperation * op) {
		for (std::deque<QueueOperation*>::iterator it = queue.begin(); it != queue.end(); ++it) {
			if (*it != op)
				continue;
			queue.erase(it);
			return true;
		}
		return false;
	}

	std::deque<QueueOperation*> m_urgent;
	std::deque<QueueOperation*> m_normal;
};

#endif //UPLOADSCHEDULINGPOLICY_H
