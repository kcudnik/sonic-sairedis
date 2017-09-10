#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <map>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#define LOG(fmt,...) printf(":- %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)

std::mutex db_mutex;

std::condition_variable cv;

volatile int run = 1; // run ntf process thread

std::queue<int> queue;

void s(int n)
{
    usleep(n * 1);
}

void process_event()
{
    LOG("before main process event");

    s(200);

    std::lock_guard<std::mutex> lock(db_mutex);

    LOG("processing main event");

    s(250);
}

std::mutex ntf_mutex;
std::unique_lock<std::mutex> ulock(ntf_mutex);

void ntf_processt()
{
    LOG("ntf process thread");

    while (run)
    {
        LOG("waiting for notification");

        cv.wait(ulock);

        std::lock_guard<std::mutex> lock(db_mutex);

        while (queue.size() != 0)
        {

            int n = queue.front();

            queue.pop();

            LOG("processing ntf %d", n);
        }

        LOG("queue is empty");

        s(50);
    }
}

void maint()
{
    LOG("starting main thread");

    while (run)
    {
        process_event();

        s(300);
    }
}

void ntf(int n)
{
    LOG("received ntf %d", n);

    // std::lock_guard<std::mutex> lock(ntf_mutex); // just to protect multiple notifications which may not be needed

    // XXX here we serialize notification and no modifications

    std::lock_guard<std::mutex> lock(db_mutex);

    LOG("putting ntf %d to queue", n);

    queue.push(n);

    LOG("signaling notifications process thread that there is something in the queue: %zu", queue.size());

    // TODO notify/signal process thread cond variable
 
    cv.notify_all();

    s(100);
}

int main(int argc, char **argv)
{
    // simulate asynchronous notifications

    std::thread main_thread = std::thread(maint);

    main_thread.detach();

    std::thread ntf_process_thread = std::thread(ntf_processt);

    ntf_process_thread.detach();

    s(1000);

    LOG("starting async ntf");

    while (1)
    {
        ntf(rand() % 5 + 1);

        s(500 *(rand()%10));
    }

    return EXIT_SUCCESS;
}
