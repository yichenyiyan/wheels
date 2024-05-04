



#include<mysql/mysql.h>
#include<string>
#include<chrono>
#include<iostream>
#include<queue>
#include<mutex>
#include<thread>
#include<condition_variable>

using namespace std;
using namespace std::chrono;

class MysqlConn {
public:
    // 初始化数据库连接
    MysqlConn();
    // 释放数据库连接
    ~MysqlConn();
    // 连接数据库
    bool connect(string user, string passwd, string dbName, string ip, unsigned short port = 3306);
    // 更新数据库: select，update，delete
    bool update(string sql);
    // 查询数据库
    bool query(string sql);
    // 遍历查询结果集
    bool ergodic();
    // 返回结果集中的某个字段的值（前提是得知道下标 下标从0开始）
    string value(int index);
    // 事务操作
    bool transaction();
    // 提交事务（关闭自动提交）
    bool commit();
    // 事务回滚
    bool rollback();
    // 刷新起始的空闲时间点
    void refreshAliveTime();
    // 计算连接存活的总时长
    long long getAliveTime();
private:
    void freeResult();
    MYSQL* m_conn = nullptr; // 数据库连接
    MYSQL_RES* m_result = nullptr;
    MYSQL_ROW m_row = nullptr;
    steady_clock::time_point m_aliveTime;
};

//初始化数据库连接
MysqlConn::MysqlConn() {
    m_conn = mysql_init(nullptr);
    mysql_set_character_set(m_conn, "UTF8");
}

//释放数据库连接
MysqlConn::~MysqlConn(){
    if (this->m_conn != nullptr) {
        mysql_close(m_conn);
    }
    freeResult();
}

// 连接数据库
bool MysqlConn::connect(string user, string passwd, string dbName, string ip, unsigned short port) {
    MYSQL* ptr = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);
    return ptr != nullptr;
}

// 更新数据库
bool MysqlConn::update(string sql) {
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    return true;
}

// 查询数据库
bool MysqlConn::query(string sql) {
    freeResult(); // 释放之前查询的结果集
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    m_result = mysql_store_result(m_conn); // 获取查询结果
    return true;
}

// 遍历查询结果集
bool MysqlConn::ergodic() {
    if (m_result != nullptr) {
        m_row = mysql_fetch_row(m_result);
        if (m_row != nullptr) {
            return true;
        }
    }
    return false;
}


// 返回结果集中的某个字段的值
string MysqlConn::value(int index) {
    int rowCount = mysql_num_fields(m_result);
    if (index >= rowCount || index < 0) {
        return string();
    }
    char* val = m_row[index];
    unsigned long length = mysql_fetch_lengths(m_result)[index];
    return string(val, length);
}

// 事务操作（关闭自动提交）
bool MysqlConn::transaction() {
    return mysql_autocommit(m_conn, false);
}

// 提交事务
bool MysqlConn::commit() {
    return mysql_commit(m_conn);
}

// 事务回滚
bool MysqlConn::rollback() {
    return mysql_rollback(m_conn);
}

// 刷新起始的空闲时间点
// 这个时间戳就是与某个数据库连接的时间起点
// 这个起点这里选择通过chrono库获得
void MysqlConn::refreshAliveTime()
{
    m_aliveTime = steady_clock::now();
}

// 计算连接存活的总时长
long long MysqlConn::getAliveTime()
{
    std::chrono::nanoseconds duration = steady_clock::now() - m_aliveTime;
    std::chrono::milliseconds millsec = duration_cast<milliseconds>(duration);
    return millsec.count();
}

//释放结果集
void MysqlConn::freeResult() {
    if (m_result != nullptr) {
        mysql_free_result(m_result);
        m_result = nullptr;
    }
}


//单例模式(这里采用懒汉模式)
class ConnPool {
public:
    static ConnPool* getConnPool();// 获得单例对象
    ConnPool(const ConnPool& obj) = delete; // 删除拷贝构造函数
    ConnPool& operator=(const ConnPool& obj) = delete; // 删除拷贝赋值运算符重载函数
    shared_ptr<MysqlConn> getConn(); // 从连接池中取出一个连接
    ~ConnPool(); // 析构函数
private:
    ConnPool(); // 构造函数私有化
    void produceConn(); // 生产数据库连接
    void recycleConn(); // 销毁数据库连接
    void addConn(); // 添加数据库连接

    // 连接服务器所需信息
    std::string m_ip;            // 数据库服务器ip地址
    std::string m_user;          // 数据库服务器用户名
    std::string m_dbName;        // 数据库服务器的数据库名
    std::string m_passwd;        // 数据库服务器密码
    unsigned short m_port;  // 数据库服务器绑定的端口

    // 连接池信息
    std::queue<MysqlConn*> m_connQ;
    unsigned int m_maxSize; // 连接数上限值
    unsigned int m_minSize; // 连接数下限值
    int m_timeout; // 连接超时时长
    int m_maxIdleTime; // 最大的空闲时长
    std::mutex q_mutex; // 独占互斥锁
    std::condition_variable m_cond; // 条件变量
};


ConnPool*
ConnPool::getConnPool() {
    static ConnPool pool;
    return &pool;
}

ConnPool::ConnPool() {
    m_ip = "127.0.0.1";
    m_user = "conn_tester";
    m_passwd = "1234";
    m_dbName = "ConnectPool";
    m_maxSize = 10;
    m_minSize = 5;
    m_timeout = 2;
    m_maxIdleTime = 10;

    for (int i = 0; i < m_minSize; ++i) {
        addConn();
    }

   
    std::thread producer(&ConnPool::produceConn, this);
    std::thread recycler(&ConnPool::recycleConn, this);
    producer.detach();
    recycler.detach();
}


void
ConnPool::addConn() {
    MysqlConn* newConn = new MysqlConn();
    if (newConn == nullptr) {
        std::cerr << "alloc a new MysqlConn error!" << std::endl;
        return;
    }
    newConn->connect(this->m_user, this->m_passwd, this->m_dbName,
        this->m_ip, m_port);
    newConn->refreshAliveTime();
    m_connQ.push(newConn);
}

void ConnPool::produceConn() {
    while (true) {  // 生产者线程不断生产连接，直到连接池达到最大值
        unique_lock<mutex> locker(q_mutex);  // 加锁，保证线程安全
        while (m_connQ.size() >= m_minSize) {
            m_cond.wait(locker);  // 等待消费者通知
        }
        addConn(); // 生产连接
        m_cond.notify_all();// 通知消费者(唤醒)
    }
}

// 回收数据库连接
void ConnPool::recycleConn() {
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(500));// 每隔半秒钟检测一次
        lock_guard<mutex> locker(q_mutex);  // 加锁，保证线程安全
        while (m_connQ.size() > m_minSize) {  // 如果连接池中的连接数大于最小连接数，则回收连接
            MysqlConn* conn = m_connQ.front();  // 取出连接池中的连接
            if (conn->getAliveTime() >= m_maxIdleTime) {
                m_connQ.pop();  // 回收连接
                delete conn;  // 释放连接资源
            }
            else {
                break;  // 如果连接的空闲时间小于最大空闲时间，则跳出循环
            }
        }
    }
}

// 从连接池中取出一个连接
shared_ptr<MysqlConn> ConnPool::getConn() {
    std::unique_lock<mutex> locker(q_mutex);
    while (m_connQ.empty()) {
        if (cv_status::timeout == m_cond.wait_for(locker, chrono::milliseconds(m_timeout))) {
            if (m_connQ.empty()) {
                //return nullptr;
                continue;
            }
        }
    }
    shared_ptr<MysqlConn>connptr(m_connQ.front(), [this](MysqlConn* conn) {
        lock_guard<mutex>locker(q_mutex); // 自动管理加锁和解锁
        conn->refreshAliveTime();// 更新连接的起始的空闲时间点
        m_connQ.push(conn); // 回收数据库连接，此时它再次处于空闲状态
        });// 智能指针
    m_connQ.pop();
    m_cond.notify_one(); // 本意是唤醒生产者
    return connptr;
}

ConnPool::~ConnPool() {
    while (!m_connQ.empty()) {
        MysqlConn* conn = m_connQ.front();
        m_connQ.pop();
        delete conn;
    }
}


void op1(int begin, int end) 
{
    for (int i = begin; i < end; ++i) {
        MysqlConn conn;
        std::string user = "conn_tester";
        std::string password = "1234";
        std::string DBName = "ConnectPool";
        std::string Hostip = "127.0.0.1";
        conn.connect(user, password, DBName, Hostip, 3306);
        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,age,sex,name) values(%d,%d,'%s','%s')",
            i, 21, "女", "大小姐");
        conn.update(sql);
    }
}

void op2(ConnPool* pool, int begin, int end) 
{
    for (int i = begin; i < end; ++i) {
        shared_ptr<MysqlConn> conn = pool->getConn();
        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,age,sex,name) values(%d,%d,'%s','%s')",
            i, 23, "女", "该丹女神");
        conn->update(sql);
    }
}

// 压力测试
void test1() 
{
#if 0
    // 非连接池，单线程
    steady_clock::time_point begin = steady_clock::now();
    op1(0, 5000);
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin; // 计算时间差，得到操作耗时
    cout << "connect pool，only a pthread，used time：" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#else
    //连接池，单线程
    ConnPool* pool = ConnPool::getConnPool();
    steady_clock::time_point begin = steady_clock::now();
    op2(pool, 0, 5000);
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin; // 计算时间差，得到操作耗时
    cout << "connect pool，only a pthread，used time：" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#endif
}

void test2() 
{
#if 0
    // 非连接池，多线程
    MysqlConn conn;
    conn.connect("heheda", "123456", "test", "127.0.0.1", 3306);
    steady_clock::time_point begin = steady_clock::now();
    thread t1(op1, 0, 1000);
    thread t2(op1, 1000, 2000);
    thread t3(op1, 2000, 3000);
    thread t4(op1, 3000, 4000);
    thread t5(op1, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin; 
    cout << "connect pool，mutil pthread，used time：" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#else 
    // 连接池，多线程
    ConnPool* pool = ConnPool::getConnPool();
    steady_clock::time_point begin = steady_clock::now();
    thread t1(op2, pool, 0, 1000);
    thread t2(op2, pool, 1000, 2000);
    thread t3(op2, pool, 2000, 3000);
    thread t4(op2, pool, 3000, 4000);
    thread t5(op2, pool, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin; 
    cout << "connect pool，mutil pthread，used time：" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#endif
}

int query() 
{
    MysqlConn conn;
    std::string user = "conn_tester";
    std::string password = "1234";
    std::string DBName = "ConnectPool";
    std::string Hostip = "127.0.0.1";
    conn.connect(user, password, DBName, Hostip, 3306);
    string sql = "insert into person values(10000,35,'女','皎月')";
    bool flag = conn.update(sql);
    if (flag) cout << "success to insert!!!" << endl;
    else cout << "fail to insert!!!" << endl;

    sql = "select * from person";
    conn.query(sql);
    while (conn.ergodic()) {
        cout << conn.value(0) << ", "
            << conn.value(1) << ", "
            << conn.value(2) << ", "
            << conn.value(3) << endl;
    }
    return 0;
}

int main() 
{
   // query();
    //test1();
    test2();
    return 0;
}