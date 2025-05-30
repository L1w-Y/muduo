#include "EventLoopThread.h"
#include"EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,const std::string &name)
:loop_(nullptr)
,exiting_(false)
,thread_(std::bind(&EventLoopThread::threadFunc,this),name)
,mutex_()
,cond_()
,callback_(cb)
{

}   

EventLoopThread::~EventLoopThread(){
    exiting_=true;
    if(loop_!=nullptr){
        loop_->quit();
        thread_.join();
    }
}   

EventLoop* EventLoopThread::startLoop(){
    thread_.start();//开起新线程
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_==nullptr){
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

/*
    这个方法传入了thread类，在thread类构造时绑定为回调函数
    在startloop启动线程时，回调触发，创建一个loop
    这也就是muduo库中one loop per thread思想
*/

void EventLoopThread::threadFunc(){
    EventLoop loop;

    if(callback_){
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}