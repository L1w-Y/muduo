#include"Acceptor.h"
#include"EventLoop.h"
#include"InetAddress.h"
#include"Logger.h"
#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<netinet/in.h>
#include"Timestamp.h"
static int createNoneblocking()
{
    int sockfd = ::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd<0){
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop,const InetAddress& listenAddr,bool reuseport)
:loop_(loop)
,acceptSocket_(createNoneblocking())
,acceptChannel_(loop,acceptSocket_.fd())
,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    //TcpServer::start()
    acceptChannel_.setReadCallback([this](Timestamp) {
            this->handleRead(); 
        });
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();

}

void Acceptor::listen(){
    listenning_ =true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead(){
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        if(newConnectionCallback_)
        {
        newConnectionCallback_(connfd,peerAddr);//轮询找到subloop，唤醒并分发新连接的channel到子loop中进行监听
        }
        else
        {
        ::close(connfd);
        }
    }else{
        LOG_ERROR("%s:%s:%d accept err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno==EMFILE){
            LOG_ERROR("%s:%s:%d sockfd reached limit!\n",__FILE__,__FUNCTION__,__LINE__);
        }
    }
}