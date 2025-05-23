#include "Poller.h"
#include"EventLoop.h"
#include"Channel.h"
Poller::Poller(EventLoop *loop):ownerLoop_(loop){}

bool Poller::hasChannel(Channel *channel) const {
    auto it = listening_channels_.find(channel->fd());
    return it!=listening_channels_.end() && it->second == channel;
}
Poller::~Poller() {}