#pragma once 
#include"noncopyable.h"
#include"Callbacks.h"
#include"Buffer.h"
#include"InetAddress.h"
#include"Timestamp.h"
#include<memory>
#include<string>
#include<atomic>
class Channel;
class EventLoop;
class Socket;


class TcpConnection :noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
        TcpConnection(EventLoop* loop,
                const std::string &name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
        ~TcpConnection();

        EventLoop* getLoop()const {return loop_;}
        const std::string& name()const{return name_;}
        const InetAddress& localAddress()const{return localAddr_;}
        const InetAddress& peerAddress()const{return peerAddr_;}
        bool connected() const{return state_ == KConnected;}

        void send(const void *message,int len);

        void shutdown();
        void connectEstablished();
        void connectDestroyed();
private:
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        void sendInloop(const void* message,size_t len);
        void shutdownInLoop();

        enum StartE{KDisconnected,KConnecting,KConnected,KDisconnecting};
        EventLoop *loop_;
        const std::string name_;
        std::atomic_int state_;
        bool reading_;

        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;

        const InetAddress localAddr_;
        const InetAddress peerAddr_;

        ConnectionCallback newConnectionCallback_;//有新连接的回调
        MessageCallback MessageCallback_;//有读写消息时的回调
        WriteCompleteCallback WriteCompleteCallback_;//消息发送完成后的回调
        HighWaterMarkCallback highwaterMarkCallback_;
        CloseCallback closeCallback_;
        size_t highWaterMark_;
        Buffer inputbuffer_;
        Buffer outputbuffer_;
};