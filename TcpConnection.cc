#include"TcpConnection.h"
#include"Logger.h"
#include"Socket.h"
#include"Channel.h"
#include"EventLoop.h"
#include<errno.h>
EventLoop* CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d tcpconnection loop is null \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}



TcpConnection::TcpConnection(EventLoop* loop,
                            const std::string &nameArg,
                            int sockfd,
                            const InetAddress& localAddr,
                            const InetAddress& peerAddr)
                            :loop_(CheckLoopNotNull(loop))
                            ,name_(nameArg)
                            ,state_(KConnecting)
                            ,reading_(true)
                            ,socket_(new Socket(sockfd))
                            ,channel_(new Channel(loop,sockfd))
                            ,peerAddr_(peerAddr)
                            ,localAddr_(localAddr)
                            ,highWaterMark_(64*1024*1024)
                            {

                                channel_->setReadCallback([this](Timestamp receiveTime){
                                    this->handleRead(receiveTime);
                                });
                                channel_->setWriteCallback([this]{
                                    this->handleWrite();
                                });
                                channel_->setCloseCallback([this]{
                                    this->handleClose();
                                });                                
                                channel_->setErrorCallback([this]{
                                    this->handleError();
                                });

                                LOG_INFO("Tcpconnection::ctor[%s]at fd=%d\n",name_.c_str(),sockfd);                            
                            }

TcpConnection::~TcpConnection(){
    LOG_INFO("Tcpconnection::dtor[%s]at fd=%d state=%d \n",name_.c_str(),channel_->fd,state_); 
}

void TcpConnection::send(const void *message,int len){}

void TcpConnection::shutdown(){}

void TcpConnection::connectEstablished(){}

void TcpConnection::connectDestroyed(){}

void TcpConnection::handleRead(Timestamp receiveTime){

    
}

void TcpConnection::handleWrite(){}

void TcpConnection::handleClose(){}

void TcpConnection::handleError(){}

void TcpConnection::sendInloop(const void* message,size_t len){}

void TcpConnection::shutdownInLoop(){}