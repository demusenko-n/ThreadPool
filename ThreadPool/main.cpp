#include <iostream>
#include "ThreadPool.h"


int main()
{
    using namespace std::chrono_literals;
    std::cout << "Thread Pool!\n";

    thread_pool pool(10);
    for (size_t i = 0; i < 15; i++)
    {
        pool.add_task([i]() {std::this_thread::sleep_for(std::chrono::seconds(i)); std::cout << "task" << i << "\n"; });
    }

    pool.wait_task(5);

    return EXIT_SUCCESS;
}