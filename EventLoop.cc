#include"EventLoop.h"
#include"Logger.h"
#include"Poller.h"
#include"Channel.h"
#include<sys/eventfd.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<memory>
__thread EventLoop* t_loopInThisThread = nullptr;

const int KPollTimeMs = 10000;

int createEventfd(){
    int evtfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);

    if(evtfd<0){
        LOG_FATAL("eventfd error: %d",errno);
    }
    return evtfd;
}

void EventLoop::loop(){
    looping_ = true;

}

EventLoop::EventLoop()
    : looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threadId_(CurrentThread::tid())
    ,poller_(Poller::newDefaultPoller(this))
    ,wakeupChannel_(new Channel(this,wakeupFd_))
    ,currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
    if(t_loopInThisThread){
        LOG_FATAL("Another loop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread=this;
    }

    //设置wakeupfd的事件类型以及发生事件后的回调
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}

void EventLoop::quit(){
    quit_=true;
    if(!isINLoopThread()){
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one =1;
    ssize_t n = read(wakeupFd_,&one,sizeof one);
    if(n !=sizeof one){
        LOG_ERROR("eventloop::handleRead() reads %d bytes instead of 8",n);
    }
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::runInLoop(Functor cb){
    if(!isINLoopThread()){
        cb();
    }
    else{
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    //唤醒执行回调的loop线程
    if(!isINLoopThread()||callingPendingFunctors_){
        wakeup();
    }
}


void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *Channel){
    return poller_->hasChannel(Channel);
}

//向wakeupfd_写一个数据
void EventLoop::wakeup(){
    uint64_t one =1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if(n!=sizeof one){
        LOG_ERROR("eventloop::wakeup() writes %lu bytes instead of 8\n",n);
    }
}

void EventLoop::dopendingFunctors(){
    std::vector<Functor> Functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        Functors.swap(pendingFunctors_);
    }

    for(auto &f : Functors){
        f();//执行当前loop回调
    }
    callingPendingFunctors_ = false;
}