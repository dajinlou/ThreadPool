#pragma once

#include <queue>
#include <pthread.h>

using namespace std;

using callback = void (*)(void* arg);
//任务结构体
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

	//添加任务
	void addTask(Task task);
	void addTask(callback f, void* arg);

	//取出任务
	Task takeTask();

	//获取当前任务的个数
	inline size_t taskNumber()
	{
		return m_taskQ.size();
	}

private:
	queue<Task> m_taskQ;   //任务队列
	pthread_mutex_t m_mutex;

};

