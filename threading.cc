#include <cerrno>
#include <iostream>

#include "threading.hh"

bool handle_join(thread_queue* tqueue);

void* cleaner(void* queue)
{
	thread_queue* tqueue = (thread_queue*)queue;
	while (1)
	{
		/* call pthread_join for completed request processing threads, clean up resources */

		if ((errno = pthread_mutex_lock(&tqueue->mutex)) != 0)
		{
			perror("pthread_mutex_lock");
			return queue;
		}
		while (!tqueue->queue.empty()) // if queue initially has items, do cleaning before waiting condition
		{
			if (!handle_join(tqueue))
			{
				std::cerr << "cleaner: failed to handle join" << std::endl;
				return queue;
			}
		}
		while (tqueue->queue.empty()) // if queue is empty, wait for condition (i.e. when queue has items)
		{
			if ((errno = pthread_cond_wait(&tqueue->condv, &tqueue->mutex)) != 0)
			{
				perror("pthread_cond_wait");
				return queue;
			}
			std::cout << "cleaner: condition signal received (i.e. queue has items)" << std::endl;
			while (!tqueue->queue.empty())
			{
				if (!handle_join(tqueue))
				{
					std::cerr << "cleaner: failed to handle join" << std::endl;
					return queue;
				}
			}
		}
		if ((errno = pthread_mutex_unlock(&tqueue->mutex)) != 0)
		{
			perror("pthread_mutex_unlock");
			return queue;
		}
	}
	return queue;
}

thread_queue create_queue()
{
	thread_queue queue;
	queue.mutex = PTHREAD_MUTEX_INITIALIZER;
	queue.condv = PTHREAD_COND_INITIALIZER;
	return queue;
}

void enter_queue(thread_queue& queue)
{
	if ((errno = pthread_mutex_lock(&queue.mutex)) != 0)
	{
		perror("pthread_mutex_lock");
		return;
	}

	queue.queue.push(pthread_self());

	if ((errno = pthread_cond_signal(&queue.condv)) != 0) // inform that queue has items
	{
		perror("pthread_cond_signal");
		return;
	}

	if ((errno = pthread_mutex_unlock(&queue.mutex)) != 0)
		perror("pthread_mutex_unlock");
}

int start_thread(void* (*routine)(void*), void* arg, std::string name)
{
	pthread_t tid;
	if ((errno = pthread_create(&tid, NULL, routine, arg)) != 0)
	{
		perror("pthread_create");
		return -1;
	}
	std::cout << "started '" << name << "' thread with ID " << tid << std::endl;
	return 0;
}

/*
 * Handle join of request processing thread
 */
bool handle_join(thread_queue* tqueue)
{
	void* result;
	if ((errno = pthread_join(tqueue->queue.front(), &result)) != 0)
	{
		perror("pthread_join");
		return false;
	}
	process_req_params* parameters = (process_req_params*)result;
	std::cout << "cleaner: thread with ID " << tqueue->queue.front() << " terminated (with parameters: "
			  << "connfd: " << parameters->connfd
			  << ", servpath: " << parameters->servpath
			  << ", dnsservip: " << parameters->dnsservip
			  << ", username: " << parameters->username
			  << ", errors: ";
	if (!parameters->errors)
		std::cout << "no";
	else
		std::cout << "yes";
	std::cout << ")" << std::endl;
	delete parameters; // free the memory that was allocated before starting the thread
	tqueue->queue.pop();
	return true;
}
