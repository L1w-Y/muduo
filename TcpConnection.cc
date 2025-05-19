#include"TcpConnection.h"
#include"Logger.h"
#include"Socket.h"
#include"Channel.h"
#include"EventLoop.h"
#include<errno.h>
#include<memory>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/tcp.h>
#include<strings.h>
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
    LOG_INFO("Tcpconnection::dtor[%s]at fd=%d state=%d \n",name_.c_str(),channel_->fd(),state_.load()); 
}

void TcpConnection::send(const void *message,int len){}
    
void TcpConnection:: shutdown(){}

void TcpConnection::connectEstablished(){}

void TcpConnection::connectDestroyed(){}

void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno = 0;
    ssize_t n = inputbuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0){
        MessageCallback_(shared_from_this(),&inputbuffer_,receiveTime);
    }else if(n == 0){
        handleClose();
    }else{
        errno = savedErrno;
        LOG_ERROR("tcpconnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite(){
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputbuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n>0){
            outputbuffer_.retrieve(n);
            if(outputbuffer_.readableBytes()==0){
                channel_->disableWriting();
                if(WriteCompleteCallback_){
                    //唤醒loop_对应的thread线程
                    loop_->queueInLoop([this] {
                        WriteCompleteCallback_(shared_from_this());
                    });
                }
                if(state_ ==KDisconnecting){
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("tcpconnection::handleWrite");
        }
    }else{
        LOG_ERROR("tcpconnection fd = %d is down,no more writing\n",channel_->fd());
    }
}

void TcpConnection::send(const std::string buf){
    if(state_ == KConnected){
        if(loop_->isINLoopThread()){
            sendInloop(buf.c_str(),buf.size());
        }
        else{
            loop_->runInLoop([this,buf = std::move(buf)]{
                this->sendInloop(buf.c_str(),buf.size());
            });
        }
    }
}
/*
    数据实际发送
    将待发送数据写入缓冲区，设置水位回调
*/
void TcpConnection::sendInloop(const void* data,size_t len){
     ssize_t nwrote = 0;
     size_t remaining = len;
     bool faultError = false;

     if(state_ == KDisconnecting){
        LOG_ERROR("disconnecting,give up writing");
        return;
     }
     //channel第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputbuffer_.readableBytes()==0){
        nwrote =::write(channel_->fd(),data,len);
        if(nwrote>0){
            remaining = len - nwrote;
            if(remaining == 0 && WriteCompleteCallback_){
                loop_->queueInLoop([this]{
                    WriteCompleteCallback_(shared_from_this());
                });
            }
        
        }
        else{
            nwrote = 0;
            if(errno != EWOULDBLOCK){
                LOG_ERROR("tcpconnection::sendinloop");
                if(errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        } 
    }
    //一次write没有全部发送数据,剩余数据保存到缓冲区中
    //注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知想要的sock->channel，调用wrirteCallback_回调方法
    //也就是调用tcpconnection：：handlewrite方法，把发送缓冲区的数据全部发送
    if(!faultError && remaining>0){
        size_t oldLen = outputbuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_
         && oldLen < highWaterMark_){
            loop_->queueInLoop([this,oldLen,remaining]{
                this->highwaterMarkCallback_(shared_from_this(),oldLen+remaining);
            });
         }
         outputbuffer_.append(static_cast<const char*>(data)+nwrote,remaining);
         if(!channel_->isWriting()){
            channel_->enableWriting();
         }
    }
}

void TcpConnection::handleClose(){
    LOG_INFO("fd=%d, state=%d\n",channel_->fd(),state_.load());
    setState(KDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    newConnectionCallback_(connPtr);
    closeCallback_(connPtr);
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("tcpconnection::handleError name%s - so_error%d \n",name_.c_str(),err);
}


void TcpConnection::shutdownInLoop(){}