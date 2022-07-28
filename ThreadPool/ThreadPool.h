#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <future>
#include <memory>

#include "Callable.h"

/**
 * \brief ThreadPool class implementation used for effective parallelism.
 */
class thread_pool
{
	void thread_main();
public:
	/**
	 * Create a new ThreadPool object
	 * \brief Constructor.
	 * \param num_threads Amount of threads in the pool.
	 */
	explicit thread_pool(size_t num_threads);

	/**
	 * \brief Destructor
	 */
	~thread_pool();

	/**
	 * \brief Add new task to queue.
	 * \param function Function to be executed.
	 * \param args Arguments to the function.
	 * \return Id of a new task.
	 */
	template<class Function, class... Args>
	std::future<std::invoke_result_t<Function, Args...>> add_task(Function function, Args&&... args);

	/**
	 * \brief Blocks calling thread until all tasks in queue are completed.
	 */
	void wait_all()const;

	/**
	 * \brief Sends command to all threads in the pool to reject all queued tasks and finish work after finishing current tasks.
	 */
	void stop_processing_tasks();

	thread_pool(const thread_pool&) = delete;
	thread_pool(thread_pool&&) = delete;
	thread_pool& operator=(thread_pool&&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;

private:
	mutable std::mutex mutex_q_;
	mutable std::condition_variable cv_new_task_;

	std::queue<std::unique_ptr<callable_abstract>> tasks_queue_;

	std::atomic_size_t tasks_completed_;
	std::atomic_size_t total_tasks_;
	std::atomic_bool is_terminated_;

	std::vector<std::jthread> threads_;
};

inline thread_pool::thread_pool(const size_t num_threads) :tasks_completed_(0), total_tasks_(0), is_terminated_(false)
{
	threads_.reserve(num_threads);
	for (size_t i = 0; i < num_threads; i++)
	{
		threads_.emplace_back([this] {thread_main(); });
	}
}

inline thread_pool::~thread_pool()
{
	stop_processing_tasks();
}

inline void thread_pool::wait_all()const
{
	if (!is_terminated_)
	{
		while (tasks_completed_ != total_tasks_)
		{
			tasks_completed_.wait(0);
		}
	}
}

inline void thread_pool::stop_processing_tasks()
{
	is_terminated_.store(true);
	cv_new_task_.notify_all();
}

template<class Function, class... Args>
std::future<std::invoke_result_t<Function, Args...>> thread_pool::add_task(Function function, Args&&... args)
{
	using return_type = std::invoke_result_t<Function, Args...>;

	std::packaged_task<return_type()> packaged_task([function, &args...]{ return function(std::forward<Args>(args)...); });
	auto future = packaged_task.get_future();

	{
		std::lock_guard l(mutex_q_);
		++total_tasks_;
		tasks_queue_.emplace(std::make_unique<callable_derived<return_type>>(std::move(packaged_task)));
	}
	cv_new_task_.notify_one();
	return future;
}

inline void thread_pool::thread_main()
{
	while (true)
	{
		std::unique_ptr<callable_abstract> task_to_complete;
		{
			std::unique_lock l(mutex_q_);
			cv_new_task_.wait(l, [this] {return !tasks_queue_.empty() || is_terminated_; });
			if (is_terminated_)
			{
				break;
			}
			task_to_complete = std::move(tasks_queue_.front());
			tasks_queue_.pop();
		}

		(*task_to_complete)();

		if (++tasks_completed_ == total_tasks_)
		{
			tasks_completed_.notify_all();
		}
	}
}