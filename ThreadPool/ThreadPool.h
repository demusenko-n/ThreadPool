#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <future>
#include <memory>
#include <functional>

/**
 * \brief ThreadPool class implementation used for effective parallelism.
 */
class thread_pool final
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
	 * \brief Add a new task to the queue.
	 * \param function Function to be executed.
	 * \param args Arguments to the function.
	 * \return std::future object to access the result of operation.
	 */
	template<class Function, class... Args>
	std::future<std::invoke_result_t<Function, Args...>> add_task(Function&& function, Args&&... args);

	/**
	 * \brief Add a new detached task to the queue.
	 * \param function Function to be executed.
	 * \param args Arguments to the function.
	 */
	template<class Function, class... Args>
	void add_detached_task(Function&& function, Args&&... args);

	/**
	 * \brief Blocks calling thread until all tasks in queue are completed.
	 */
	void wait_all()const;

	thread_pool(const thread_pool&) = delete;
	thread_pool(thread_pool&&) = delete;
	thread_pool& operator=(thread_pool&&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;

private:
	mutable std::mutex mutex_q_;
	mutable std::condition_variable cv_new_task_;
	std::queue<std::move_only_function<void()>> tasks_queue_;

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
	wait_all();
	is_terminated_.store(true);
	cv_new_task_.notify_all();
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

template<class Function, class... Args>
std::future<std::invoke_result_t<Function, Args...>> thread_pool::add_task(Function&& function, Args&&... args)
{
	using return_type = std::invoke_result_t<Function, Args...>;
	
	std::packaged_task<return_type()> packaged_task(std::bind(std::forward<Function>(function), std::forward<Args>(args)...));
	auto future = packaged_task.get_future();

	{
		std::lock_guard l(mutex_q_);
		++total_tasks_;

		tasks_queue_.emplace([move_only_task = std::move(packaged_task)]()mutable{move_only_task(); });
	}
	cv_new_task_.notify_one();
	return future;
}

inline void thread_pool::thread_main()
{
	while (true)
	{
		std::move_only_function<void()> task_to_complete;
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

		task_to_complete();

		if (++tasks_completed_ == total_tasks_)
		{
			tasks_completed_.notify_all();
		}
	}
}

template<class Function, class... Args>
void thread_pool::add_detached_task(Function&& function, Args&&... args)
{
	{
		std::lock_guard l(mutex_q_);
		++total_tasks_;
		tasks_queue_.emplace(std::bind(std::forward<Function>(function), std::forward<Args>(args)...));
	}
	cv_new_task_.notify_one();
}
