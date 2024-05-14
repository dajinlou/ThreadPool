#pragma once
#include "TaskQueue.h"

class ThreadPool
{
public:
	//创建线程池并初始化
	ThreadPool(int min, int max);

	//销毁线程池
	~ThreadPool();

	//给线程池添加任务
	void addTask(Task task);

	//获取线程池中工作线程的个数
	int getBusyNum();

	//获取线程池中活着的线程个数
	int getAliveNum();

private:
	//工作的线程任务函数
	static void* worker(void* arg);

	//管理者线程任务函数
	static void* manager(void* arg);

	//单个线程退出
	void threadExit();

private:
	TaskQueue* taskQ;
	pthread_t managerID;
	pthread_t* threadIDs;   //工作的线程ID
	int minNum;
	int maxNum;
	int busyNum;
	int liveNum;
	int exitNum;
	pthread_mutex_t mutexPool;
	pthread_cond_t notEmpty;

	bool shutdown=false;  //是不是要销毁线程池
};

