#include"Buffer.h"
#include<errno.h>
#include<sys/uio.h>
#include<unistd.h>
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    // 1. 在栈上创建一个临时缓冲区，大小为 65536 字节 (64KB)
    char extrabuf[65536] = {}; 

    // 2. 定义一个 iovec 结构体数组，用于 readv 系统调用
    //    iovec 结构体用于描述一块内存区域 (基地址 + 长度)
    //    readv 可以一次性从文件描述符读取数据到多个不连续的内存区域
    struct iovec vec[2];

    // 3. 获取当前 Buffer 中可写字节数
    const size_t writable = writableBytes();

    // 4. 设置第一个 iovec 结构体：指向 Buffer 内部的可写空间
    vec[0].iov_base = begin() + writeIndex_; // 可写区域的起始地址
    vec[0].iov_len = writable;              // 可写区域的长度

    // 5. 设置第二个 iovec 结构体：指向栈上的临时缓冲区 extrabuf
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;       // extrabuf 的总大小
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;

    // 7. 调用 readv 系统调用从 fd 读取数据到 vec 指定的内存区域
    //    readv 会先尝试填满 vec[0]，如果还有数据且iovcnt是2，再尝试填满 vec[1]
    const ssize_t n = ::readv(fd, vec, iovcnt);

    // 8. 处理 readv 的返回值
    if (n < 0) {
        // 读取出错，保存错误码
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 读取成功，并且所有读取到的数据都成功存入了 Buffer 内部的可写空间 (vec[0])
        // 直接增加写指针 writeIndex_
        writeIndex_ += n;
    } else {
        // 读取成功，但读取到的数据量 n 超过了 Buffer 内部初始的可写空间 writable
        // 这意味着数据一部分存入了 vec[0]，剩下的存入了 extrabuf (vec[1])
        // 首先，Buffer 内部的可写空间已全部被填满
        writeIndex_ = buffer_.size(); // 将写指针移到 Buffer 的末尾
        // 然后，将 extrabuf 中超出 writable 部分的数据追加到 Buffer 中
        // n - writable 是实际存储在 extrabuf 中的数据量
        // append 函数会负责处理 Buffer 空间的扩展
        append(extrabuf, n - writable);
    }

    // 9. 返回实际读取到的字节数 n
    return n;
}

ssize_t Buffer::writeFd(int fd,int *saveErrno){
     size_t n = ::write(fd,peek(),readableBytes());
     if(n < 0){
        *saveErrno = errno;
    }
    return n;
}
