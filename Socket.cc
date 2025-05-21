#include"Socket.h"
#include"Logger.h"
#include"InetAddress.h"
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <netinet/tcp.h>
#include<strings.h>
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
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr,sizeof addr);
    int connfd = ::accept4(sockfd_,(sockaddr*)&addr,&len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd>0){
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite(){
    if(::shutdown(sockfd_,SHUT_WR)<0){
        LOG_ERROR("shutdownWrite error");
    }
}
// 关闭 Nagle 算法，提高小包传输效率
void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    // IPPROTO_TCP 表示 TCP 层，TCP_NODELAY 用于启用/关闭 Nagle 算法
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
    // 设置为 1 表示关闭 Nagle 算法，数据立即发送，适用于低延迟需求场景
}

// 允许端口重用（处于TIME_WAIT状态时仍可绑定）
void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    // SOL_SOCKET 表示 socket 层，SO_REUSEADDR 用于地址复用
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    // 启用后，服务器可在关闭后立即重启绑定同一端口
}

// 允许多个 socket 同时绑定同一端口（提高并发能力）
void Socket::setReusePort(bool on) {
    int optval = on ? 1 : 0;
    // SOL_SOCKET 表示 socket 层，SO_REUSEPORT 用于端口复用
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
    // 启用后，多线程或多进程可分别拥有自己的 socket，提高连接分发效率
}

//启用 TCP 保活机制，检测对端是否断开连接
void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    // SOL_SOCKET 表示 socket 层，SO_KEEPALIVE 用于启用 TCP 保活
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
    // 启用后，空闲连接将定期发送探测包，判断对端是否仍然在线
}