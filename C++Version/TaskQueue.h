#pragma once

#include <queue>
#include <pthread.h>

using namespace std;

using callback = void (*)(void* arg);
//����ṹ��
struct Task
{
	Task()
	{
		function = nullptr;
		arg = nullptr;
	}
	Task(callback f, void* arg)
	{
		this->arg = arg;
		function = f;
	}
	callback function;
	void* arg;
};

class TaskQueue
{
public:
	TaskQueue();
	~TaskQueue();

	//�������
	void addTask(Task task);
	void addTask(callback f, void* arg);

	//ȡ������
	Task takeTask();

	//��ȡ��ǰ����ĸ���
	inline size_t taskNumber()
	{
		return m_taskQ.size();
	}

private:
	queue<Task> m_taskQ;   //�������
	pthread_mutex_t m_mutex;

};

