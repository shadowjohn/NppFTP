#ifndef QUEUETERMINALLIFECYCLE_H
#define QUEUETERMINALLIFECYCLE_H

template <typename OperationT, typename EventT>
auto queue_end_and_take_terminal(OperationT * operation, EventT endEvent) -> decltype(operation->OnQueueTerminal())
{
	operation->SendNotification(endEvent);
	return operation->OnQueueTerminal();
}

template <typename OperationT>
auto queue_cancel_and_take_terminal(OperationT * operation) -> decltype(operation->OnQueueTerminal())
{
	operation->OnQueueCanceled();
	return operation->OnQueueTerminal();
}

template <typename OperationT>
OperationT * queue_filter_terminal_follow_up(OperationT * followUp, bool suppress)
{
	if (!suppress)
		return followUp;
	delete followUp;
	return NULL;
}

#endif //QUEUETERMINALLIFECYCLE_H
