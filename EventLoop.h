#pragma once
#include"noncopyable.h"
#include"Timestamp.h"
#include"CurrentThread.h"
#include<memory>
#include<functional>
#include<vector>
#include<atomic>
#include<mutex>
class Channel;
class Poller;

class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();
    
    void loop();//开起事件循环
    void quit();//退出事件循环

    Timestamp pollReturnTime() const{return pollReturnTime_;}
    void runInLoop(Functor cb);//在当前loop执行cb
    void queueInLoop(Functor cb);//把cb放入队列，幻想loop所在线程，执行cb

    void wakeup();//唤醒loop所在线程
    /*
        ==》调用poller的方法
    */
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *Channel);
    
    bool isINLoopThread()const {return threadId_==CurrentThread::tid();}
private:

    void handleRead();//wake up
    void dopendingFunctors();//执行回调

    using ChannelList = std::vector<Channel*>;
    std::atomic_bool looping_;
    std::atomic_bool quit_;


    const pid_t threadId_;//当前loop所在的线程id
    Timestamp pollReturnTime_;//poller返回的发生时间的channels的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;//当mainloop获取新的channel，轮询选择subloop,通过wakeupFd_唤醒subloop
    std::unique_ptr<Channel> wakeupChannel_;//eventloop不操作fd，统一封装为channel，wakeupChannel_封装wakeupFd_

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_;//当前loop是否有需要执行的回调
    std::vector<Functor> pendingFunctors_;//储存loop所需要的所有回调操作
    std::mutex mutex_;//线程安全
};

