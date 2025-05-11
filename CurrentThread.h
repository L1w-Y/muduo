#pragma once
#include<unistd.h>
#include<sys/syscall.h>

namespace CurrentThread{
    extern __thread int t_cachedTid;
    void cacheTid();
    
    inline int tid(){
        //预计 t_cachedTid == 0 很少发生
        if(__builtin_expect(t_cachedTid == 0,0)){
            cacheTid();
        }
        return t_cachedTid;
    }
}