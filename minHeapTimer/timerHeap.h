#ifndef MY_TIMEHEAP_H_
#define MY_TIMEHEAP_H_

#include <time.h>
#include <iostream>
#include <netinet/in.h>


const int BUFFER_SZIE = 64;

class HeapTimer;


//用户数据
struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SZIE];
    HeapTimer* timer;
};


//定时器类
class HeapTimer{
public:
    time_t expire;
    client_data* user_data;

    HeapTimer(int delay){
        expire = time(NULL) + delay;
    }
    void (*cb_func)(client_data*);
};


class TimeHeap{
private:
    HeapTimer** array;  //堆数组
    int capacity;       //堆数组容量
    int cur_size;       //堆数组当前包含元素个数
public:
    TimeHeap(int cap);
    TimeHeap(HeapTimer** init_array, int size, int cap);
    ~TimeHeap();

    void percolate_down(int hole);  //对堆结点进行下滤
    void add_timer(HeapTimer* timer);
    void del_timer(HeapTimer* timer);
    void pop_timer();
    void tick();

    void resize();

};



#endif