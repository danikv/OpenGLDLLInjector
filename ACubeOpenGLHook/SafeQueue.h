#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
template <class T>
class SafeQueue
{
public:
    SafeQueue(void)
    : q()
    , m()
    , c()
    {}

    // Add an element to the queue.
    void enqueue(T&& t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push_back(t);
        c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(m);
        c.wait(lock, [&] { return !q.empty(); });
        T val = std::move(q.front());
        q.pop_front();
        return val;
    }

private:

    std::deque<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};