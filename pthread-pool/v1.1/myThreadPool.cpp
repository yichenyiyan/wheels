/******************************
*  Github:yichenyiyan  QAQ    *
*  							  *
*  							  *
*  							  *
*******************************/




#include<iostream>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include<cassert>
#include <condition_variable>
#include <future>
#include <chrono>
#include <functional>
#include <stdexcept>   





class ThreadPool {
public:
    ThreadPool( size_t threadNumber = 4 );
    template< class F, class... Args >
    auto addTask(F&& f, Args&&... args)
        -> std::future< typename std::result_of<F(Args...) >::type>;
    ~ThreadPool();
private:
    // vector储存线程
    std::vector< std::thread* > workers;
    //任务队列
    std::queue< std::function<void()> > tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    int size;
};


ThreadPool::ThreadPool( size_t threadNumber )
    : stop( false ), size( threadNumber )
{
    //初始化
    for (size_t i = 0; i < threadNumber; ++i)
        workers.emplace_back(new std::thread(
            [this]()
            {
                while (true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            }
    ));
}


template<class F, class... Args>
auto ThreadPool::addTask(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task< return_type() > >(
        std::bind( std::forward<F>(f), std::forward<Args>(args)... )
    );

    std::future< return_type > res = task->get_future();
    {
        std::unique_lock< std::mutex > lock( this->queue_mutex );

        //线程池停止时不应该再往里面插入任务啦
        if (stop)
            throw std::runtime_error("add task on stopped ThreadPool");

        tasks.emplace([ task ]() { ( *task )(); });
    }
    condition.notify_one();
    return res;
}


ThreadPool::~ThreadPool()
{
    {
        std::unique_lock< std::mutex > lock( this->queue_mutex );
        stop = true;
    }
    condition.notify_all();
    for ( auto worker : workers ) {
        if ( worker->joinable() )
            worker->join();
        delete worker;
    }
    workers.clear();
}

void waitMe()
{
    std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
    std::cout << "DEBUD: before END..." << std::endl;
}

void testFunc1()
{
    ThreadPool* pool = new ThreadPool();
    assert(pool != nullptr);
    for (int i = 0; i < 16; ++i) {
        pool->addTask(waitMe);
    }

    std::this_thread::sleep_for(std::chrono::seconds( 22 ));

    delete pool;
}



int  main()
{
    testFunc1();

    std::cout << "->END..." << std::endl;

    return 0;
}

