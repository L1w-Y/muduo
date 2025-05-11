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

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels){

    LOG_INFO("func=%s => fd total count:%zu\n",__FUNCTION__,listening_channels_.size());
    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);

    int saveErrno = errno;
    Timestamp now(Timestamp::now());
    if(numEvents > 0){
        LOG_INFO("%d events happened\n",numEvents);
        fillActiveChannels(numEvents , activeChannels);
        //如果所有事件都触发了，就进行扩容
        if(numEvents == events_.size()){
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents == 0){
        LOG_DEBUG("%s timeout \n",__FUNCTION__);
    }
    else{
        if(saveErrno != EINTR){
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll（） error !");
        }
    }
    return now;

}
/*

channel更新update和remove，
通知eventloop来调用poller的updatechannel和removechannel方法

*/
void EPollPoller::updateChannel(Channel *channel){
    const int index = channel->index();
    LOG_INFO("func = %s fd=%d events=%d index=%d \n",__FUNCTION__,channel->fd(),channel->events(),index);
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
    LOG_INFO("func = %s fd=%d\n",__FUNCTION__,fd);
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


void EPollPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels)const{

    for(int i = 0; i < numEvents; ++i){
        auto channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revent(events_[i].events);
        activeChannels->emplace_back(channel);//将发生事件的channel给到eventloop，指针传递
    }

}