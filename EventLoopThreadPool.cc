#include"EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
:baseLoop_(baseLoop)
,name_(nameArg)
,started_(false)
,numThreads_(0)
,next_(0)
{

}

EventLoopThreadPool::~EventLoopThreadPool(){

}

void EventLoopThreadPool::start(const ThreadInitCallback &cb){
    
}

EventLoop* EventLoopThreadPool::getNextLoop(){

}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){

}

