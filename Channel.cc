#include "Channel.h" 
#include<sys/epoll.h>
#include"EventLoop.h"
#include"Logger.h"
#include"Timestamp.h"
const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent= EPOLLOUT;

Channel::Channel(EventLoop *loop,int fd):loop_(loop),fd_(fd),
events_(0),revents_(0),index_(-1)
{

}
Channel::~Channel(){

}

void Channel::tie(const std::shared_ptr<void>&obj){
    tie_=obj;
    tied_=true;
}

void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        auto ptr = tie_.lock();
        if(ptr){
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}

void Channel::update(){
  loop_->updateChannel(this);
}

void Channel::handleEventWithGuard(Timestamp recieveTime){
    //关闭写端会触发epollhup事件
    if((revents_ & EPOLLHUP)&&!(revents_ & EPOLLIN)){
        if(closeCallback_)closeCallback_();
    }
    if(revents_ & EPOLLERR){
        if(errorCallback_)errorCallback_();
    }
    if(revents_&(EPOLLIN | EPOLLPRI)){
        if(readCallback_)readCallback_(recieveTime);
    }
    if(revents_ & EPOLLOUT){
        if(writeCallback_)writeCallback_();
    }
}

void Channel::remove(){
   loop_->removeChannel(this);
}