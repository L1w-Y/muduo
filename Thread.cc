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
    sem_init(&sem,false,0);// 信号量初始化为 0
    thread_ = std::make_shared<std::thread>([&](){
        tid_ = CurrentThread::tid();// 在新线程中获取线程 ID
        sem_post(&sem);// 通知主线程继续
        if(func_)func_();// 执行线程的主要任务
    });
    sem_wait(&sem);// 主线程等待新线程完成初始化
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