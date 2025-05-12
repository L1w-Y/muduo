#include"Thread.h"
#include"CurrentThread.h"
#include<semaphore.h>
std::atomic<int> Thread::numCreated_{0};

Thread::Thread(ThreadFunc func,const std::string &name)
:started_(false),
 joined_(false),
 tid_(0),
 func_(std::move(func)),
 name_(name)
{
    setDefaultName();
}

Thread::~Thread(){
    if(started_ && !joined_){
        thread_->detach();
    }
}
void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem,false,0);
    thread_ = std::make_shared<std::thread>([&](){
        //获取tid
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        if(func_)func_();
    });
    sem_wait(&sem);
}

void Thread::join(){
    joined_=true;
    thread_->join();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if(name_.empty()){
        char buf[32]={};
        snprintf(buf,sizeof buf,"thread %d", num);
        name_=buf;
    }
}