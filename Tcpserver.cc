#include "Tcpserver.h"
#include"Logger.h"
#include<strings.h>
#include<functional>
#include"TcpConnection.h"
static EventLoop* CheckLoopNotNull(EventLoop *loop){
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


/*
*   TcpServer::start()后内部listen监听连接
*   监听用的fd封装为channel注册到了Eventloop中
*   当listen监听到读事件触发时，Eventloop通知Channel
*   属于Channel的readCallback_（也就是Acceptor::handleRead()）触发
*   handleRead()中完成accpet（）获得新连接fd和ip地址端口号
*   再通过回调回调，是由TcpServer的构造中绑定为 TcpServer::newConnection，传回给tcpServer
*   在newConnection完成新连接的分发
*
*/

void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr){
    EventLoop *ioloop = threadPool_->getNextLoop();
    char buf[64] = {};
    snprintf(buf,sizeof buf,"%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("tcpserver::newConnection [%s]  new connection [%s] from %s \n",
        name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

    sockaddr_in local;
    ::bzero(&local,sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd,(sockaddr *)&local,&addrlen) < 0){
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(ioloop,connName,sockfd,localAddr,peerAddr);
    connections_[connName] = conn;
    conn->setConnectionCallback(newConnectionCallback_);
    conn->setMessageCallback(MessageCallback_);
    conn->setWriteCompleteCallback(WriteCompleteCallback_);

    conn->setCloseCallback(
        [this](const TcpConnectionPtr& conn) {
            this->removeConnection(conn);
        }
    );

    ioloop->runInLoop([conn]() {
        conn->connectEstablished();
    });
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn){

}



void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){

}