#include "ThreadPool.h"
#include <pthread.h>


const int NUMBER = 2;

//任务结构体
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
}Task;

//线程池结构体
struct ThreadPool
{
	//任务队列
	Task* taskQ;
	int queueCapacity;  //容量
	int queueSize;      //当前任务个数
	int queueFront;     //队头  用于取数据
	int queueRear;      //队尾  用于添加数据
	 
	//线程
	pthread_t managerID;    //管理者线程ID
	pthread_t* threadIDs;   //工作的线程ID
	int minNum;				//最小线程数量
	int maxNum;				//最大线程数量
	int busyNum;			//忙的线程的个数
	int liveNum;			//存活的线程的个数
	int exitNum;			//要销魂的线程个数

	//2个锁
	pthread_mutex_t mutexPool;   //锁整个线程池
	pthread_mutex_t mutexBusy;	 //锁busyNum变量
	//2个条件变量
	pthread_cond_t notFull;		 //任务队列是不是满了
	pthread_cond_t notEmpty;     //任务队列是不是空了


	int shutdown;    //是不是要销毁线程池，销毁为1，不销毁为0
 };

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	//创建的内存，要给其他地方使用，所以要创建为堆内存
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do
	{
		if (NULL == pool) {
			printf("malloc threadpool fail....\n");
			break;
		}

		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (NULL == pool->threadIDs)
		{
			printf("malloc threadIDs fail...\n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);

		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;   //和最小个数相等
		pool->exitNum = 0;

		//互斥锁 和 条件变量
		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			printf("mutex cond  fail ...\n");
			break;
		}

		//任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;   //实际任务个数
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = 0;


		//创建线程
		pthread_create(&pool->managerID, NULL, manager, pool);
		for (int i = 0; i < min; ++i) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}

		return pool;
	} while (0);


	//释放资源
	if (pool && pool->threadIDs) {
		free(pool->threadIDs);
	}
	if (pool && pool->taskQ) {
		free(pool->taskQ);
	}
	if (pool) {
		free(pool); 
	}

	return NULL;
}

int threadPoolDestory(ThreadPool* pool)
{
	if (NULL == pool) {
		return -1;
	}
	//关闭线程池
	pool->shutdown = 1;
	//阻塞回收管理者线程
	pthread_join(pool->managerID, NULL);

	//唤醒阻塞的消费者线程
	for (int i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	}
	//释放堆内存
	if (pool->taskQ) {
		free(pool->taskQ);
	}
	if (pool->threadIDs) {
		free(pool->threadIDs);
	}
	
	//互斥锁 + 条件变量
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notFull);
	pthread_cond_destroy(&pool->notEmpty);
	free(pool);
	pool = NULL;
	
	return 0;
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
		//阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	//判断线程池是否被关闭
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	//添加任务
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	//移动结点
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;
	//唤醒消费者消费
	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int alibeNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return alibeNum;
}


//工作函数
void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		//当前任务队列是否为空
		while (pool->queueSize == 0 && !pool->shutdown) {
			//阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);  //经历四个阶段  阻塞 --> 解锁 --->唤醒--->加锁
		    
			//判断是不是要销毁线程
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}	
				//pthread_exit(NULL);
			}
		}

		//判断线程池是否被关闭了
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool);
			//pthread_exit(NULL);
			threadExit(pool);
		}
		//从任务队列中取出一个任务    消费者
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;

		//移动头结点
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity; //下一个结点位置
		pool->queueSize--;
		//唤醒生产者
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		//执行任务前
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		//开始执行任务
		task.function(task.arg);
		free(task.arg);
		task.arg = NULL;

		//任务执行完成后
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}


}

void* manager(void* arg)
{
	//它的职责   创建和销毁线程
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		//每隔3s检测一次
		sleep(3);

		//取出线程池中任务的数量和当前线程的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;    //实际任务个数
		int liveNum = pool->liveNum;        //线程存活个数
		pthread_mutex_unlock(&pool->mutexPool);

		//取出忙线程的数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;    //实际任务个数
		pthread_mutex_unlock(&pool->mutexBusy);

		//添加线程    任务的个数>存活的线程个数  && 存活的线程数<最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum) {
			int counter = 0;
			pthread_mutex_lock(&pool->mutexPool);
			//从0开始找一个空闲位置
			for (int i = 0; i < pool->maxNum && counter < NUMBER&&pool->liveNum<pool->maxNum; ++i) {
				if (pool->threadIDs[i] == 0) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
				
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		//销毁线程     忙的线程*2 < 存活的线程 && 存活的线程>最小线程数
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {

			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			//让工作的线程自杀
			for (int i = 0; i < NUMBER; ++i) {
				pthread_cond_signal(&pool->notEmpty);
			}

		}

	}


	return NULL;
}

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; ++i) {
		if (pool->threadIDs[i] == tid) {
			pool->threadIDs[i] = 0;
			printf("threadExit() called, %ld exiting....\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}
