#pragma once



template<class T>
struct Node{
	T data;
	Node<T>* next;
};

template<class T, int blockSize = 10>
class myMemoryPool {
private:
	Node<T>* pUseHead;    //����ʹ�õĽڵ�
	Node<T>* pMemoryHead; //δʹ�õĽڵ�
public:
	int headCount;
	int usedCount;

	myMemoryPool();
	~myMemoryPool();

	void* myAllocate();
	int myFree(void*);

};