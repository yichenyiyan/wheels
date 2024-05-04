#include "memory.h"

template<class T, int blockSize>
myMemoryPool<T, blockSize>::myMemoryPool()
	:pMemoryHead(nullptr), pUseHead(nullptr)
	, headCount(0), usedCount(0)
{
	
}




template<class T, int blockSize>
myMemoryPool<T, blockSize>::~myMemoryPool()
{
	Node<T>* ptr;
	while (this->pMemoryHead) {
		ptr = this->pMemoryHead;
		delete ptr;
		this->pMemoryHead = this->pMemoryHead->next;
	}

	while (this->pUseHead) {
		ptr = this->pUseHead;
		delete ptr;
		this->pUseHead = this->pUseHead->next;
	}
}


template<class T, int blockSize>
void* 
myMemoryPool<T, blockSize>::myAllocate()
{
	//没有空闲节点
	if (this->pMemoryHead == nullptr) {
		this->pMemoryHead = new Node<T>;

		//将数据连接起来
		Node<T>* pTmp = this->pMemoryHead;
		for (int i = 1; i < blockSize; ++i) {
			pTmp->next = new Node<T>;
			pTmp = pTmp->next;
		}
		pTmp->next = nullptr;

		this->headCount += blockSize;
	}

	Node<T>* pTmp = this->pMemoryHead;
	this->pMemoryHead = pMemoryHead->next;

	//将data挂在到UseHead
	pTmp->next = this->pUseHead;
	this->pUseHead = pTmp;
	this->headCount--;
	this->usedCount++;

	return reinterpret_cast<void*>(pTmp);
}

template<class T, int blockSize>
int 
myMemoryPool<T, blockSize>::myFree(void* p)
{
	Node<T>* pFind = (Node<T>*)p;
	Node<T>* pTmp = this->pUseHead;

	if (pTmp == pFind) {
		this->pUseHead = this->pUseHead->next;
		pTmp->next = this->pMemoryHead;
		this->pMemoryHead = pTmp;

		this->usedCount--;
		this->headCount++;
		return 1;
	}

	Node<T>* prev;
	while (pTmp != nullptr) {
		if (pTmp == pFind) {
			prev->next = pTmp->next;
			pTmp->next = this->pMemoryHead;
			this->pMemoryHead = pTmp;

			this->usedCount--;
			this->headCount++;

			return 1;
		}
		prev = pTmp;
		pTmp = pTmp->next;
	}

	return -1;
}