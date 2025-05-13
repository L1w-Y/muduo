#include"Socket.h"
#include"Logger.h"
#include"InetAddress.h"
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>

Socket::~Socket(){
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr){
    if(0 != ::bind(sockfd_,(sockaddr *)localaddr.getSockAddr(),sizeof(sockaddr_in))){
        LOG_FATAL("bind socket:%d fail\n",sockfd_);
    }
}
void Socket::listen(){
    if(0 != ::listen(sockfd_,1024)){
         LOG_FATAL("listen socket:%d fail\n",sockfd_);
    }
}
int Socket::accept(InetAddress *peeraddr){
    
}

void Socket::shutdownWrite(){
    
}
void Socket::setTcpNoDelay(bool on){
    
}
void Socket::setReuseAddr(bool on){
    
}
void Socket::setReusePort(bool on){
    
}
void Socket::setKeepAlive(bool on){
    
}