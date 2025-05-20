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


class TcpConnection :noncopyable , public std::enable_shared_from_this<TcpConnection>
{
private:
        enum StateE {KDisconnected,KConnecting,KConnected,KDisconnecting};
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

        void setConnectionCallback(const ConnectionCallback& cb){ ConnectionCallback_ = cb; }

        void setMessageCallback(const MessageCallback& cb){ MessageCallback_ = cb; }

        void setCloseCallback(const CloseCallback& cb){closeCallback_ = cb;}
        void setWriteCompleteCallback(const WriteCompleteCallback& cb){ WriteCompleteCallback_ = cb; }

        void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark){ 
                highwaterMarkCallback_ = cb; 
                highWaterMark_ = highWaterMark; 
                }
        
        void shutdown();
        void connectEstablished();
        void connectDestroyed();
        void setState(StateE state){state_=state;}
private:
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        void send(const std::string buf);

        void sendInloop(const void* data,size_t len);
        void shutdownInLoop();

        EventLoop *loop_;
        const std::string name_;
        std::atomic_int state_;
        bool reading_;

        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;

        const InetAddress localAddr_;
        const InetAddress peerAddr_;

        ConnectionCallback ConnectionCallback_;//有新连接的回调
        MessageCallback MessageCallback_;//有读写消息时的回调
        WriteCompleteCallback WriteCompleteCallback_;//消息发送完成后的回调
        HighWaterMarkCallback highwaterMarkCallback_;
        CloseCallback closeCallback_;
        size_t highWaterMark_;
        Buffer inputbuffer_;
        Buffer outputbuffer_;
};