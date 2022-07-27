#include <iostream>
#include "ThreadPool.h"


int main()
{
    using namespace std::chrono_literals;
    std::cout << "Thread Pool!\n";

    ThreadPool pool(10);
    for (size_t i = 0; i < 15; i++)
    {
        pool.addTask([i]() {std::this_thread::sleep_for(std::chrono::seconds(i)); std::cout << "task" << i << "\n"; });
    }

    pool.waitTask(5);

    return EXIT_SUCCESS;
}