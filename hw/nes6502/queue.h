#include <stdio.h>
#include <stdlib.h>  

#define MAXSIZE 10000 /* 存储空间初始分配量 */

typedef int Status;
typedef int ElemType; /* ElemType类型根据实际情况而定，这里假设为int */

/* 顺序循环队列的顺序存储结构 */
typedef struct
{
	ElemType data[MAXSIZE];
	int front; // 头指针
	int rear; // 尾指针，若队列不空，指向队列尾元素的下一个位置
}SeqQueue;

Status initQueue(SeqQueue *Q); // 初始化队列操作
Status enQueue(SeqQueue *Q, const ElemType e); // 入队操作
Status deQueue(SeqQueue *Q, ElemType *e); // 出队操作
Status tarverseQueue(const SeqQueue Q); // 遍历队列操作
Status clearQueue(SeqQueue *Q); // 清空队列操作
Status isEmpty(const SeqQueue Q); // 判断是否为空队列
Status getHead(const SeqQueue Q, ElemType *e); // 获得队头元素
int getLength(const SeqQueue Q); // 获得队列的长度

// 初始化队列操作
Status initQueue(SeqQueue *Q)
{
	Q->front = 0;
	Q->rear = 0;

	return  1;
}

// 入队操作
Status enQueue(SeqQueue *Q, const ElemType e)
{
	// 判断队列是否已满
	if ((Q->rear + 1) % MAXSIZE == Q->front) 
		return 0;

	Q->data[Q->rear] = e; // 将元素e赋值给队尾
	Q->rear = (Q->rear + 1) % MAXSIZE; // rear指针向后移一位置，若到最后则转到数组头部

	return  1;
}

// 出队操作
Status deQueue(SeqQueue *Q, ElemType *e)
{
	// 判断是否为空队
	if (Q->front == Q->rear) 
		return 0;

	*e = Q->data[Q->front]; // 将队头元素赋值给e
	Q->front = (Q->front + 1) % MAXSIZE; // front指针向后移一位置,若到最后则转到数组头部

	return  1;
}

// 遍历队列操作
Status tarverseQueue(const SeqQueue Q)
{
	int cur = Q.front; // 当前指针
	while (cur != Q.rear) // 直到cur指向了队尾元素的下一个位置，即Q.rear，结束循环
	{
		printf("%d ", Q.data[cur]);
		cur = (cur + 1) % MAXSIZE; // 当前指针向后推移
	}
	printf("\n");

	return 1;
}

// 清空队列操作
Status clearQueue(SeqQueue *Q)
{
	Q->front = Q->rear = 0;

	return 1;
}

// 判断是否为空队列
Status isEmpty(const SeqQueue Q)
{
	return Q.front == Q.rear ? 1 : 0;
}

// 获得队头元素
Status getHead(const SeqQueue Q, ElemType *e)
{
	if (Q.front == Q.rear) // 判断是否为空队列
		return 0;
	*e = Q.data[Q.front];

	return 1;
}

// 获得队列的长度
int getLength(const SeqQueue Q)
{
	return  (Q.rear - Q.front + MAXSIZE) % MAXSIZE;
}
