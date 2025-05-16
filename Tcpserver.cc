#include "Tcpserver.h"
#include"Logger.h"
#include<functional>
EventLoop* CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d mainloop is null \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop *loop,const InetAddress &listenAddr,const std::string &nameArg,Option option)
: loop_(CheckLoopNotNull(loop))
,ipPort_(listenAddr.toIpPort())
,name_(nameArg)
,acceptor_(new  Acceptor(loop,listenAddr,option == KReusePort))
,threadPool_(new EventLoopThreadPool(loop,name_))
,newConnectionCallback_()
,MessageCallback_()
,nextConnId_(1)
{
    //新用户连接时，执行tcpserver::newconnection回调
    acceptor_->setNewConnectionCallback(
    [this](int sockfd, const InetAddress& peerAddr) {
        newConnection(sockfd, peerAddr);
    });
}

TcpServer::~TcpServer(){

}

//设置底层subloop个数
void TcpServer::setThreadNum(int number){
    threadPool_->setThreadNum(number);
}

//开起服务器监听
void TcpServer::start(){
    if(started_++ == 0)//防止一个tcpserver对象start多次
    {
        threadPool_->start(threadInitCallback_); //启动loop线程池
        loop_->runInLoop([this]() {   //主loop
            acceptor_->listen();
        });
    }
}

void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr){

}

void TcpServer::removeConncetion(const TcpConnectionPtr &conn){

}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){

}