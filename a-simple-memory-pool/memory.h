#pragma once



template<class T>
struct Node{
	T data;
	Node<T>* next;
};

template<class T, int blockSize = 10>
class myMemoryPool {
private:
	Node<T>* pUseHead;    //正在使用的节点
	Node<T>* pMemoryHead; //未使用的节点
public:
	int headCount;
	int usedCount;

	myMemoryPool();
	~myMemoryPool();

	void* myAllocate();
	int myFree(void*);

};