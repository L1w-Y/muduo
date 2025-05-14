#pragma once
#include"noncopyable.h"
#include"Socket.h"
#include"Channel.h"
#include<functional>
class Eventloop;
class InetAddress;
class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void (int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop,const InetAddress& listenAddr,bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb){
        newConnectionCallback_ = std::move(cb);
    }

    bool listenning()const{return listenning_;}
    void listen();

private:
    void handleRead();
    EventLoop *loop_; //使用的是用户定义的baseloop（mainloop）,用来接收tcp新连接
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};