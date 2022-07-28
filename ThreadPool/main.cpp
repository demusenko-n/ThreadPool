#include <iostream>
#include <any>
#include "ThreadPool.h"


std::string some_function(int& ref, int val)
{
	std::this_thread::sleep_for(std::chrono::seconds(5));

	ref += val;
	return std::string(2, 'a');
}

std::unique_ptr<int> sh()
{
	return std::make_unique<int>(7);

}

int main()
{
	using namespace std::chrono_literals;
	std::cout << "Thread Pool!\n";

	thread_pool pool(10);

	{
		int test = 10;
		auto future = pool.add_task(some_function, test, 5);

		auto future2 = pool.add_task(sh);

		pool.add_detached_task([] {std::this_thread::sleep_for(std::chrono::seconds(10)); std::cout << "10 seconds passed"; });
		

		std::cout << future.get() << std::endl;

		std::cout << *future2.get() << std::endl;


		std::cout << test << std::endl;


	}
	std::cout << "exited {}" << std::endl;
	pool.wait_all();


	return EXIT_SUCCESS;
}