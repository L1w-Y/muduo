#pragma once 
#include"EventLoop.h"
#include"Acceptor.h"
#include"InetAddress.h"
#include"noncopyable.h"
#include"EventLoopThreadPool.h"
#include"Callbacks.h"
#include<functional>
#include<string>
#include<memory>
#include<atomic>
#include<unordered_map>
class TcpServer: noncopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    enum Option{
        KNoReusePort,
        KReusePort,
    };

    TcpServer(EventLoop *loop,const InetAddress &listenAddr,const std::string &nameArg,Option option = KNoReusePort);
    ~TcpServer();
    void setThreadInitCallback(const ThreadInitCallback &cb){ threadInitCallback_ = std::move(cb);}
    void setNewConnectionCallback(const ConnectionCallback &cb){ newConnectionCallback_ = std::move(cb);}
    void setMessageCallback(const MessageCallback &cb){MessageCallback_=std::move(cb);}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){WriteCompleteCallback_=std::move(cb);}
    //设置底层subloop个数
    void setThreadNum(int number);
    //开起服务器监听
    void start();
private:
    void newConnection(int sockfd,const InetAddress &peerAddr);
    void removeConncetion(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string,TcpConnectionPtr>;

    EventLoop *loop_;//用户定义的loop
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;//运行在mainloop，监听新连接
    std::unique_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback newConnectionCallback_;//有新连接的回调
    MessageCallback MessageCallback_;//有读写消息时的回调
    WriteCompleteCallback WriteCompleteCallback_;//消息发送完成后的回调

    ThreadInitCallback threadInitCallback_;//loop线程初始化的回调
    std::atomic_int started_;
    int nextConnId_;
    ConnectionMap connections_;//保存所有的连接
};