#pragma once 
#include "noncopyable.h"
#include <unordered_map>
#include<vector>
#include"Timestamp.h"
class Channel;
class EventLoop;

class Poller : noncopyable{

public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    virtual Timestamp poll(int timeoutMs, Channel *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    bool hasChannel(Channel *channel)const;
    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    //key：shckfd，value：sockfd所属的Channel类型
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;
};