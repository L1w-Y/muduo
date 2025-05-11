#pragma once
#include "noncopyable.h"
#include<functional>
#include<memory>
class EventLoop;
class Timestamp;
class Channel : noncopyable
{
public:
    //事件回调
    using EventCallback = std::function<void()>;
    //只读事件的回调
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel(EventLoop *loop,int fd);
    ~Channel();
    //fd 得到poller通知以后，处理事件
    void handleEvent(Timestamp receiveTime);
    void remove();
    //设置回调
    void setReadCallback(ReadEventCallback cb){ readCallback_=std::move(cb); }
    void setWriteCallback(EventCallback cb){ writeCallback_=std::move(cb); }
    void setCloseCallback(EventCallback cb){ closeCallback_=std::move(cb); }
    void setErrorCallback(EventCallback cb){ errorCallback_=std::move(cb); }

    //  防止当channel被手动remove掉，channel还在执行回调
    void tie(const std::shared_ptr<void>&obj);
    //获取fd
    int fd(){return fd_;}
    //获取事件
    int events()const{return events_;}
    //设置返回的具体事件
    void set_revent(int revt){revents_=revt;}

    void enableReading(){events_ |= KReadEvent;   update();}
    void disableReading(){events_ |= ~KReadEvent; update();}
    void enableWriting(){events_ |= KWriteEvent;  update();}
    void disableWriting(){events_ |= ~KWriteEvent;  update();}
    void disableAll(){events_=KNoneEvent;update();}
    
    bool isNoneEvent()const {return events_ == KNoneEvent;}
    bool isWriting()const {return events_&KWriteEvent;}
    bool isReading()const {return events_ &KReadEvent;}
    int index(){return index_;}
    void set_index(int idx){index_=idx;}

    EventLoop* ownerLoop(){return loop_;}
private:

    void update();
    void handleEventWithGuard(Timestamp recieveTime);

    static const int KNoneEvent;
    static const int KReadEvent;
    static const int KWriteEvent;

    EventLoop *loop_; //channel所属的事件循环
    const int fd_; //fd，poller监听的对象
    int events_; //注册fd感兴趣的事件,在channel中进行设置
    int revents_; //poller返回的具体发生的事件，poller中返回时修改
    int index_;  //

    std::weak_ptr<void> tie_{};
    bool tied_{};

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};