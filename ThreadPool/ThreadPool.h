#pragma once
#include <vector>
#include <thread>
#include <functional>	
#include <atomic>
#include <unordered_set>
#include <queue>
#include <shared_mutex>
#include <condition_variable>

/**
 * \brief ThreadPool class implementation used for effective parallelism
 */
class ThreadPool
{
	struct Task
	{
		std::function<void()> executable{};
		int64_t taskId{};
	};

	void threadMain()
	{
		while (true)
		{
			Task taskToComplete;
			{
				std::unique_lock l(mutex_q_);
				cv_new_task_.wait(l, [this] {return !tasksQueue_.empty(); });
				if (isTerminated_)
				{

					break;
				}
				taskToComplete = std::move(tasksQueue_.front());
				tasksQueue_.pop();
			}

			taskToComplete.executable();

			{
				std::lock_guard l(mutex_s_);
				completedTaskIds_.emplace(taskToComplete.taskId);
			}
			cv_completed_task_.notify_all();
		}
	}

	bool isValidTaskId(int64_t taskId)const;
public:
	/**
	 * Create a new ThreadPool object
	 * \brief Constructor.
	 * \param numThreads Amount of threads in the pool.
	 */
	ThreadPool(size_t numThreads);

	/**
	 * \brief Move constructor.
	 * \param threadPool Thread Pool to be moved.
	 */
	ThreadPool(ThreadPool&& threadPool) = default;

	/**
	 * \brief Move assignment operator.
	 * \param threadPool Thread Pool to be moved.
	 */
	ThreadPool& operator=(ThreadPool&& threadPool) = default;

	/**
	 * \brief Destructor
	 */
	~ThreadPool();

	/**
	 * \brief Add new task to queue.
	 * \param function Function to be executed.
	 * \param args Arguments to the function.
	 * \return Id of a new task.
	 */
	template<class Func, class... Args>
	int64_t addTask(Func function, Args&&... args);

	/**
	 * \brief Determine if the task is completed.
	 * \param taskId Id of the task.
	 * \return Whether the task with the specified id is completed.
	 */
	bool isCompleted(int64_t taskId)const;

	/**
	 * \brief Blocks calling thread until all tasks in queue are completed.
	 */
	void waitAll()const;

	/**
	 * \brief Blocks calling thread until the specified task is completed.
	 * \param taskId
	 */
	void waitTask(int64_t taskId)const;

	/**
	 * \brief Sends command to all threads in the pool to reject all queued tasks and finish work after finishing current tasks.
	 * \param taskId
	 */
	void stopProcessingTasks();

	ThreadPool(const ThreadPool& other) = delete;
	ThreadPool& operator=(const ThreadPool& threadPool) = default;
private:
	mutable std::mutex mutex_q_;
	mutable std::mutex mutex_s_;

	mutable std::condition_variable cv_new_task_;
	mutable std::condition_variable cv_completed_task_;

	std::queue<Task> tasksQueue_;
	std::unordered_set<int64_t> completedTaskIds_;

	std::atomic<int64_t> lastTaskId_;
	std::atomic_bool isTerminated_;

	std::vector<std::jthread> threads_;
};

ThreadPool::ThreadPool(const size_t numThreads) :lastTaskId_(0), isTerminated_(false)
{
	threads_.reserve(numThreads);
	for (size_t i = 0; i < numThreads; i++)
	{
		threads_.emplace_back([this]() {threadMain(); });
	}
}

ThreadPool::~ThreadPool()
{
	stopProcessingTasks();
}

void ThreadPool::waitAll()const
{
	std::unique_lock l(mutex_s_);
	cv_completed_task_.wait(l, [this] {return completedTaskIds_.size() == lastTaskId_; });
}

void ThreadPool::waitTask(const int64_t taskId)const
{
	if (isValidTaskId(taskId))
	{
		std::unique_lock l(mutex_s_);
		cv_completed_task_.wait(l, [this, taskId] {return completedTaskIds_.contains(taskId); });
	}
}

void ThreadPool::stopProcessingTasks()
{
	isTerminated_.store(true);
	{
		std::lock_guard l(mutex_q_);
		tasksQueue_.emplace();
	}
	cv_new_task_.notify_all();
}

template<class Func, class... Args>
int64_t ThreadPool::addTask(const Func function, Args&&... args)
{
	{
		std::lock_guard l(mutex_q_);
		tasksQueue_.emplace(Task{ std::bind(function, std::forward<Args>(args)...), ++lastTaskId_ });
	}
	cv_new_task_.notify_one();
	return lastTaskId_;
}

bool ThreadPool::isCompleted(const int64_t taskId)const
{
	if (!isValidTaskId(taskId))
	{
		return false;
	}

	std::lock_guard l(mutex_s_);
	return completedTaskIds_.contains(taskId);
}

bool ThreadPool::isValidTaskId(const int64_t taskId)const
{
	return taskId <= lastTaskId_ && taskId > 0;
}