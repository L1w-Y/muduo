#include "EPollPoller.h"
#include"Channel.h"
#include<errno.h>
#include"Logger.h"
#include <string.h>
#include <unistd.h>
//表示channel未添加到poller中
const int KNew = -1; //channel的index初始化是-1
//channel已添加到poller中
const int KAdded = 1;
//channel从poller中删除
const int KDeleted =2;

EPollPoller::EPollPoller(EventLoop *loop)
:Poller(loop),
epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
events_(KInitEventListSize)
{
    if(epollfd_<0){
        LOG_FATAL("epoll_create error:%d \n",errno);
    }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, Channel *activeChannels){

}
/*

channel更新update和remove，
通知eventloop来调用poller的updatechannel和removechannel方法

*/
void EPollPoller::updateChannel(Channel *channel){
    const int index = channel->index();
    LOG_INFO("fd=%d events=%d index=%d \n",channel->fd(),channel->events(),index);
    if(index == KNew || index== KDeleted){
        if(index == KNew){
            int fd = channel->fd();
            listening_channels_[fd]=channel;
        }
        channel->set_index(KAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else{ //channel已经注册过
        int fd = channel->fd();
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(KDeleted);
        }
        else{
            update(EPOLL_CTL_MOD,channel);
        }
    }
}

//从poller中删除channel，不再监听
void EPollPoller::removeChannel(Channel *channel){
    int fd=channel->fd();
    listening_channels_.erase(fd);

    int index = channel->index();
    if(index==KAdded) update(EPOLL_CTL_DEL,channel);

    channel->set_index(KNew);

}


void EPollPoller::update(int operation,Channel *channel){

    epoll_event event;
    memset(&event,0,sizeof event);
    int fd = channel->fd();
    event.data.fd = fd;
    event.events = channel->events();
    event.data.ptr = channel;
    
    if(::epoll_ctl(epollfd_,operation,fd,&event)<0){
        if(operation==EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else{
            LOG_ERROR("epoll_ctl add/mod error:%d\n",errno);
        }
    }
}