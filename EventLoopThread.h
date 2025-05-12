#pragma once
#include"noncopyable.h"
#include<functional>
#include<mutex>
#include<condition_variable>
#include"Thread.h"
class EventLoop;
class EventLoopThread : noncopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
private:
    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};