/******************************
*  作者：拿得起更放得下(CSDN) *
*  Github:大小姐的小迷弟      *
*       yichenyan  QAQ        *
* 编程学习应该是简单且有趣的  *
*******************************/



#include<iostream>
#include<thread>
#include<vector>
#include<queue>
#include<mutex>
#include<cassert>
#include<chrono>
#include<atomic> //原子操作
#include<functional>
#include<condition_variable>





using namespace std;

atomic<int> Number = 0;


class myThreadPool {
private:
	mutex poolmtx;				//互斥锁
	condition_variable poolcv; //条件变量
	atomic<int> size;               //线程池大小
	atomic<bool> stop;              //是否停止
	vector<thread*> threads;         //线程
	queue<function<void()>> tasks;  //任务队列
public:
	myThreadPool(int size = 4);    //默认四个线程
	~myThreadPool();
	bool push_task(function<void()>);
};

myThreadPool::myThreadPool(int size)
	:size(size), stop(false)
{
	//线程初始化
	for (int i = 0; i < size; ++i) {
		threads.emplace_back(new thread([&]() {
			function<void()> cb;
			while (true) {
				{
					unique_lock<mutex> lck(this->poolmtx);
					this->poolcv.wait(lck, [&]() {return this->stop || !this->tasks.empty(); });

					if (this->stop && tasks.empty())
						return;

					cb = move(tasks.front());
					tasks.pop();

				}
				cb();
				}
			}));
	}
}



myThreadPool::~myThreadPool()
{
	{
		unique_lock<mutex> lck(this->poolmtx);
		this->stop = true;
	}

	this->poolcv.notify_all();
	for (int i = 0; i < threads.size(); ++i) {
		if (threads[i]->joinable())
			threads[i]->join();
	}
	this->threads.clear();

	while (!tasks.empty()) {
		tasks.pop();
	}

}



bool 
myThreadPool::push_task(function<void()> cb)
{
	auto src_size = this->tasks.size();
	{
		unique_lock<mutex> lck(this->poolmtx);
		this->tasks.push(move(cb));
		if(src_size == this->tasks.size() - 1)
			this->poolcv.notify_one();
	}
	
	return src_size == this->tasks.size() - 1;
}


void toSleep5Sec()
{
	cout << "No." << Number << endl;
	std::this_thread::sleep_for(std::chrono::seconds(5));
	cout << "No." << Number << "finish" << endl;
}

void testFunc1()
{
	function<void()> cb = bind(::toSleep5Sec);
	myThreadPool* pool = new myThreadPool(5);
	assert(pool != nullptr);
	for (int i = 0; i < 5; ++i) {
		pool->push_task(cb);
	}
	std::this_thread::sleep_for(std::chrono::seconds(7));

	delete pool;
}



int main()
{
	testFunc1();
	cout << "如果toSleep5Sec()函数的内容比这条先打印完，那么逻辑上说明线程都正常工作了逻辑上" << endl;
	return 0;
}