



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
    // ��ʼ�����ݿ�����
    MysqlConn();
    // �ͷ����ݿ�����
    ~MysqlConn();
    // �������ݿ�
    bool connect(string user, string passwd, string dbName, string ip, unsigned short port = 3306);
    // �������ݿ�: select��update��delete
    bool update(string sql);
    // ��ѯ���ݿ�
    bool query(string sql);
    // ������ѯ�����
    bool ergodic();
    // ���ؽ�����е�ĳ���ֶε�ֵ��ǰ���ǵ�֪���±� �±��0��ʼ��
    string value(int index);
    // �������
    bool transaction();
    // �ύ���񣨹ر��Զ��ύ��
    bool commit();
    // ����ع�
    bool rollback();
    // ˢ����ʼ�Ŀ���ʱ���
    void refreshAliveTime();
    // �������Ӵ�����ʱ��
    long long getAliveTime();
private:
    void freeResult();
    MYSQL* m_conn = nullptr; // ���ݿ�����
    MYSQL_RES* m_result = nullptr;
    MYSQL_ROW m_row = nullptr;
    steady_clock::time_point m_aliveTime;
};

//��ʼ�����ݿ�����
MysqlConn::MysqlConn() {
    m_conn = mysql_init(nullptr);
    mysql_set_character_set(m_conn, "UTF8");
}

//�ͷ����ݿ�����
MysqlConn::~MysqlConn(){
    if (this->m_conn != nullptr) {
        mysql_close(m_conn);
    }
    freeResult();
}

// �������ݿ�
bool MysqlConn::connect(string user, string passwd, string dbName, string ip, unsigned short port) {
    MYSQL* ptr = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);
    return ptr != nullptr;
}

// �������ݿ�
bool MysqlConn::update(string sql) {
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    return true;
}

// ��ѯ���ݿ�
bool MysqlConn::query(string sql) {
    freeResult(); // �ͷ�֮ǰ��ѯ�Ľ����
    if (mysql_query(m_conn, sql.c_str())) {
        return false;
    }
    m_result = mysql_store_result(m_conn); // ��ȡ��ѯ���
    return true;
}

// ������ѯ�����
bool MysqlConn::ergodic() {
    if (m_result != nullptr) {
        m_row = mysql_fetch_row(m_result);
        if (m_row != nullptr) {
            return true;
        }
    }
    return false;
}


// ���ؽ�����е�ĳ���ֶε�ֵ
string MysqlConn::value(int index) {
    int rowCount = mysql_num_fields(m_result);
    if (index >= rowCount || index < 0) {
        return string();
    }
    char* val = m_row[index];
    unsigned long length = mysql_fetch_lengths(m_result)[index];
    return string(val, length);
}

// ����������ر��Զ��ύ��
bool MysqlConn::transaction() {
    return mysql_autocommit(m_conn, false);
}

// �ύ����
bool MysqlConn::commit() {
    return mysql_commit(m_conn);
}

// ����ع�
bool MysqlConn::rollback() {
    return mysql_rollback(m_conn);
}

// ˢ����ʼ�Ŀ���ʱ���
// ���ʱ���������ĳ�����ݿ����ӵ�ʱ�����
// ����������ѡ��ͨ��chrono����
void MysqlConn::refreshAliveTime()
{
    m_aliveTime = steady_clock::now();
}

// �������Ӵ�����ʱ��
long long MysqlConn::getAliveTime()
{
    std::chrono::nanoseconds duration = steady_clock::now() - m_aliveTime;
    std::chrono::milliseconds millsec = duration_cast<milliseconds>(duration);
    return millsec.count();
}

//�ͷŽ����
void MysqlConn::freeResult() {
    if (m_result != nullptr) {
        mysql_free_result(m_result);
        m_result = nullptr;
    }
}


//����ģʽ(�����������ģʽ)
class ConnPool {
public:
    static ConnPool* getConnPool();// ��õ�������
    ConnPool(const ConnPool& obj) = delete; // ɾ���������캯��
    ConnPool& operator=(const ConnPool& obj) = delete; // ɾ��������ֵ��������غ���
    shared_ptr<MysqlConn> getConn(); // �����ӳ���ȡ��һ������
    ~ConnPool(); // ��������
private:
    ConnPool(); // ���캯��˽�л�
    void produceConn(); // �������ݿ�����
    void recycleConn(); // �������ݿ�����
    void addConn(); // ������ݿ�����

    // ���ӷ�����������Ϣ
    std::string m_ip;            // ���ݿ������ip��ַ
    std::string m_user;          // ���ݿ�������û���
    std::string m_dbName;        // ���ݿ�����������ݿ���
    std::string m_passwd;        // ���ݿ����������
    unsigned short m_port;  // ���ݿ�������󶨵Ķ˿�

    // ���ӳ���Ϣ
    std::queue<MysqlConn*> m_connQ;
    unsigned int m_maxSize; // ����������ֵ
    unsigned int m_minSize; // ����������ֵ
    int m_timeout; // ���ӳ�ʱʱ��
    int m_maxIdleTime; // ���Ŀ���ʱ��
    std::mutex q_mutex; // ��ռ������
    std::condition_variable m_cond; // ��������
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
    while (true) {  // �������̲߳����������ӣ�ֱ�����ӳشﵽ���ֵ
        unique_lock<mutex> locker(q_mutex);  // ��������֤�̰߳�ȫ
        while (m_connQ.size() >= m_minSize) {
            m_cond.wait(locker);  // �ȴ�������֪ͨ
        }
        addConn(); // ��������
        m_cond.notify_all();// ֪ͨ������(����)
    }
}

// �������ݿ�����
void ConnPool::recycleConn() {
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(500));// ÿ�������Ӽ��һ��
        lock_guard<mutex> locker(q_mutex);  // ��������֤�̰߳�ȫ
        while (m_connQ.size() > m_minSize) {  // ������ӳ��е�������������С�����������������
            MysqlConn* conn = m_connQ.front();  // ȡ�����ӳ��е�����
            if (conn->getAliveTime() >= m_maxIdleTime) {
                m_connQ.pop();  // ��������
                delete conn;  // �ͷ�������Դ
            }
            else {
                break;  // ������ӵĿ���ʱ��С��������ʱ�䣬������ѭ��
            }
        }
    }
}

// �����ӳ���ȡ��һ������
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
        lock_guard<mutex>locker(q_mutex); // �Զ���������ͽ���
        conn->refreshAliveTime();// �������ӵ���ʼ�Ŀ���ʱ���
        m_connQ.push(conn); // �������ݿ����ӣ���ʱ���ٴδ��ڿ���״̬
        });// ����ָ��
    m_connQ.pop();
    m_cond.notify_one(); // �����ǻ���������
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
            i, 21, "Ů", "��С��");
        conn.update(sql);
    }
}

void op2(ConnPool* pool, int begin, int end) 
{
    for (int i = begin; i < end; ++i) {
        shared_ptr<MysqlConn> conn = pool->getConn();
        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,age,sex,name) values(%d,%d,'%s','%s')",
            i, 23, "Ů", "�õ�Ů��");
        conn->update(sql);
    }
}

// ѹ������
void test1() 
{
#if 0
    // �����ӳأ����߳�
    steady_clock::time_point begin = steady_clock::now();
    op1(0, 5000);
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin; // ����ʱ���õ�������ʱ
    cout << "connect pool��only a pthread��used time��" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#else
    //���ӳأ����߳�
    ConnPool* pool = ConnPool::getConnPool();
    steady_clock::time_point begin = steady_clock::now();
    op2(pool, 0, 5000);
    steady_clock::time_point end = steady_clock::now();
    auto length = end - begin; // ����ʱ���õ�������ʱ
    cout << "connect pool��only a pthread��used time��" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#endif
}

void test2() 
{
#if 0
    // �����ӳأ����߳�
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
    cout << "connect pool��mutil pthread��used time��" << length.count() << " ns,"
        << length.count() / 1000000 << " ms" << endl;
#else 
    // ���ӳأ����߳�
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
    cout << "connect pool��mutil pthread��used time��" << length.count() << " ns,"
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
    string sql = "insert into person values(10000,35,'Ů','���')";
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