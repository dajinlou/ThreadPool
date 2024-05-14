#pragma once
#include "TaskQueue.h"

class ThreadPool
{
public:
	//�����̳߳ز���ʼ��
	ThreadPool(int min, int max);

	//�����̳߳�
	~ThreadPool();

	//���̳߳��������
	void addTask(Task task);

	//��ȡ�̳߳��й����̵߳ĸ���
	int getBusyNum();

	//��ȡ�̳߳��л��ŵ��̸߳���
	int getAliveNum();

private:
	//�������߳�������
	static void* worker(void* arg);

	//�������߳�������
	static void* manager(void* arg);

	//�����߳��˳�
	void threadExit();

private:
	TaskQueue* taskQ;
	pthread_t managerID;
	pthread_t* threadIDs;   //�������߳�ID
	int minNum;
	int maxNum;
	int busyNum;
	int liveNum;
	int exitNum;
	pthread_mutex_t mutexPool;
	pthread_cond_t notEmpty;

	bool shutdown=false;  //�ǲ���Ҫ�����̳߳�
};

