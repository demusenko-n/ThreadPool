#pragma once
#include <vector>
#include <thread>
#include <functional>	
#include <atomic>
#include <unordered_set>
#include <queue>
#include <condition_variable>

/**
 * \brief ThreadPool class implementation used for effective parallelism
 */
class thread_pool
{
	struct task
	{
		std::function<void()> executable{};
		size_t task_id{};
	};

	void thread_main();

	bool is_valid_task_id(size_t task_id)const;
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
	template<class Func, class... Args>
	size_t add_task(Func function, Args&&... args);

	/**
	 * \brief Determine if the task is completed.
	 * \param task_id Id of the task.
	 * \return Whether the task with the specified id is completed.
	 */
	bool is_completed(size_t task_id)const;

	/**
	 * \brief Blocks calling thread until all tasks in queue are completed.
	 */
	void wait_all()const;

	/**
	 * \brief Blocks calling thread until the specified task is completed.
	 * \param task_id Id of the task
	 */
	void wait_task(size_t task_id)const;

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
	mutable std::mutex mutex_s_;

	mutable std::condition_variable cv_new_task_;
	mutable std::condition_variable cv_completed_task_;

	std::queue<task> tasks_queue_;
	std::unordered_set<size_t> completed_task_ids_;

	std::atomic<size_t> last_task_id_;
	std::atomic_bool is_terminated_;

	std::vector<std::jthread> threads_;
};

inline thread_pool::thread_pool(const size_t num_threads) :last_task_id_(0), is_terminated_(false)
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
	std::unique_lock l(mutex_s_);
	cv_completed_task_.wait(l, [this] {return completed_task_ids_.size() == last_task_id_; });
}

inline void thread_pool::wait_task(const size_t task_id)const
{
	if (is_valid_task_id(task_id))
	{
		std::unique_lock l(mutex_s_);
		cv_completed_task_.wait(l, [this, task_id] {return completed_task_ids_.contains(task_id); });
	}
}

inline void thread_pool::stop_processing_tasks()
{
	is_terminated_.store(true);
	{
		std::lock_guard l(mutex_q_);
		tasks_queue_.emplace();
	}
	cv_new_task_.notify_all();
}

template<class Func, class... Args>
size_t thread_pool::add_task(const Func function, Args&&... args)
{
	{
		std::lock_guard l(mutex_q_);
		tasks_queue_.emplace(task{[function, &args...] { return function(std::forward<Args>(args)...); }, ++last_task_id_ });
	}
	cv_new_task_.notify_one();
	return last_task_id_;
}

inline bool thread_pool::is_completed(const size_t task_id)const
{
	if (!is_valid_task_id(task_id))
	{
		return false;
	}

	std::lock_guard l(mutex_s_);
	return completed_task_ids_.contains(task_id);
}

inline bool thread_pool::is_valid_task_id(const size_t task_id)const
{
	return task_id <= last_task_id_ && task_id > 0;
}

inline void thread_pool::thread_main()
{
	while (true)
	{
		task task_to_complete;
		{
			std::unique_lock l(mutex_q_);
			cv_new_task_.wait(l, [this] {return !tasks_queue_.empty(); });
			if (is_terminated_)
			{

				break;
			}
			task_to_complete = std::move(tasks_queue_.front());
			tasks_queue_.pop();
		}

		task_to_complete.executable();

		{
			std::lock_guard l(mutex_s_);
			completed_task_ids_.emplace(task_to_complete.task_id);
		}
		cv_completed_task_.notify_all();
	}
}