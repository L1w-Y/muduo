#pragma once
#include"Poller.h"
#include<vector>
#include<sys/epoll.h>
#include"Timestamp.h"
/*
    对epoll实际操作的封装：
        epoll_create
        epoll_ctl
        epoll_wait
*/
class EPollPoller : public Poller
{
public:
    //构造中进行epoll_create
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override ;
    //epoll_wait操作
    Timestamp poll(int timeoutMs, ChannelList *activeChannels)override;
    //epoll_ctl操作
    void updateChannel(Channel *channel)override;
    void removeChannel(Channel *channel) override;
private:
    static const int KInitEventListSize = 16;

    void fillActiveChannels(int numEvents,ChannelList *activeChannels)const;
    void update(int operation,Channel *channel);

    using EventList = std::vector<epoll_event>;
    int epollfd_; //epoll句柄
    EventList events_;
};
