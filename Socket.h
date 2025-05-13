#pragma once
#include"noncopyable.h"
class InetAddress;

class Socket:noncopyable
{
public:
    explicit Socket(int sockfd):sockfd_(sockfd){}  // 构造函数，通过给定的 sockfd 创建 Socket 对象
    ~Socket();  

    int fd()const {return sockfd_;}  // 获取套接字文件描述符

    void bindAddress(const InetAddress &localaddr);  // 绑定本地地址
    void listen();  // 监听连接请求
    int accept(InetAddress *peeraddr);  // 接受连接请求，获取对端地址

    void shutdownWrite();  // 关闭写端
    void setTcpNoDelay(bool on);  // 设置 TCP NoDelay 选项
    void setReuseAddr(bool on);  // 设置地址重用选项
    void setReusePort(bool on);  // 设置端口重用选项
    void setKeepAlive(bool on);  // 设置保持连接选项
private:
    const int sockfd_; 
};;