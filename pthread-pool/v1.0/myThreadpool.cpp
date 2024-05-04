/******************************
*  ���ߣ��õ�����ŵ���(CSDN) *
*  Github:��С���С�Ե�      *
*       yichenyan  QAQ        *
* ���ѧϰӦ���Ǽ�����Ȥ��  *
*******************************/



#include<iostream>
#include<thread>
#include<vector>
#include<queue>
#include<mutex>
#include<cassert>
#include<chrono>
#include<atomic> //ԭ�Ӳ���
#include<functional>
#include<condition_variable>





using namespace std;

atomic<int> Number = 0;


class myThreadPool {
private:
	mutex poolmtx;				//������
	condition_variable poolcv; //��������
	atomic<int> size;               //�̳߳ش�С
	atomic<bool> stop;              //�Ƿ�ֹͣ
	vector<thread*> threads;         //�߳�
	queue<function<void()>> tasks;  //�������
public:
	myThreadPool(int size = 4);    //Ĭ���ĸ��߳�
	~myThreadPool();
	bool push_task(function<void()>);
};

myThreadPool::myThreadPool(int size)
	:size(size), stop(false)
{
	//�̳߳�ʼ��
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
	cout << "���toSleep5Sec()���������ݱ������ȴ�ӡ�꣬��ô�߼���˵���̶߳������������߼���" << endl;
	return 0;
}