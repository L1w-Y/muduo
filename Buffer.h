#pragma once 
#include<vector>
#include<string>
#include<algorithm>
/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode


class Buffer {
public:
    // 预留的头部空间，方便前置数据写入（例如在数据前插入协议头）
    static const size_t KCheapPrepend = 8;

    // 缓冲区的初始大小（不包括前置区）
    static const size_t KInitialSize = 1024;

    // 构造函数，初始化 buffer_ 并设置读写指针到 KCheapPrepend 后的位置
    explicit Buffer(size_t initialSize = KInitialSize)
        : buffer_(KCheapPrepend + initialSize),
          readerIndex_(KCheapPrepend),
          writeIndex_(KCheapPrepend) {}

    // 返回可读字节数（写指针 - 读指针）
    size_t readableBytes() const {
        return writeIndex_ - readerIndex_;
    }

    // 返回可写字节数（缓冲区总大小 - 写指针）
    size_t writableBytes() const {
        return buffer_.size() - writeIndex_;
    }

    // 返回前置空间的大小（读指针之前的区域,包含头部）
    size_t prependableBytes() const {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const {
        return begin() + readerIndex_;
    }

    // 读取 len 长度的数据，相当于向前推进 readerIndex_
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            // 只推进部分读指针
            readerIndex_ += len;
        } else {
            // 如果要读的长度超过了可读数据，则全部清空
            retrieveAll();
        }
    }

    // 清空所有数据，重置读写指针
    void retrieveAll() {
        readerIndex_ = writeIndex_ = KCheapPrepend;
    }

    // 读取所有可读数据并以 std::string 返回，同时清空缓冲区
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    // 读取 len 长度的数据并返回为 string，同时推进读指针
    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len); // 从 peek 开始复制 len 字节
        retrieve(len);                   // 读取后移动读指针
        return result;
    }

    void ensureWriteableBytes(size_t len){
        if(writableBytes()<len){
            makeSpace(len);
        }
    }

    void append(const char *data,size_t len){
        ensureWriteableBytes(len);
        std::copy(data,data+len,beginWrite());
        writeIndex_ += len;
    }

    char* beginWrite(){
        return begin() + writeIndex_;
    }
    const char* beginWrite() const {
        return begin() + writeIndex_;
    }
    //从fd上读取数据
    ssize_t readFd(int fd,int* savedErrno);
private:
    // 返回 buffer_ 的起始地址（非 const 版本）
    char* begin() {
        // it.operator*()访问容器第一个元素本身
        // & 取地址，得到第一个元素的位置
        return &*buffer_.begin();
    }
    // 返回 buffer_ 的起始地址（const 版本）
    const char* begin() const {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len){
        if(writableBytes()+prependableBytes() < len + KCheapPrepend){
            buffer_.resize(writeIndex_+len);
        }else{
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin()+writeIndex_, begin()+KCheapPrepend);
            readerIndex_=KCheapPrepend;
            writeIndex_= readerIndex_+readable;
        }
    }

    std::vector<char> buffer_;    // 实际缓冲区
    size_t readerIndex_;    // 读指针：指向可读数据的起始位置
    size_t writeIndex_;    // 写指针：指向写入数据的起始位置
};