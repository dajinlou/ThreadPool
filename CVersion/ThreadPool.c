#include "ThreadPool.h"
#include <pthread.h>


const int NUMBER = 2;

//����ṹ��
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
}Task;

//�̳߳ؽṹ��
struct ThreadPool
{
	//�������
	Task* taskQ;
	int queueCapacity;  //����
	int queueSize;      //��ǰ�������
	int queueFront;     //��ͷ  ����ȡ����
	int queueRear;      //��β  ������������
	 
	//�߳�
	pthread_t managerID;    //�������߳�ID
	pthread_t* threadIDs;   //�������߳�ID
	int minNum;				//��С�߳�����
	int maxNum;				//����߳�����
	int busyNum;			//æ���̵߳ĸ���
	int liveNum;			//�����̵߳ĸ���
	int exitNum;			//Ҫ������̸߳���

	//2����
	pthread_mutex_t mutexPool;   //�������̳߳�
	pthread_mutex_t mutexBusy;	 //��busyNum����
	//2����������
	pthread_cond_t notFull;		 //��������ǲ�������
	pthread_cond_t notEmpty;     //��������ǲ��ǿ���


	int shutdown;    //�ǲ���Ҫ�����̳߳أ�����Ϊ1��������Ϊ0
 };

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	//�������ڴ棬Ҫ�������ط�ʹ�ã�����Ҫ����Ϊ���ڴ�
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
		pool->liveNum = min;   //����С�������
		pool->exitNum = 0;

		//������ �� ��������
		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			printf("mutex cond  fail ...\n");
			break;
		}

		//�������
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;   //ʵ���������
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = 0;


		//�����߳�
		pthread_create(&pool->managerID, NULL, manager, pool);
		for (int i = 0; i < min; ++i) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}

		return pool;
	} while (0);


	//�ͷ���Դ
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
	//�ر��̳߳�
	pool->shutdown = 1;
	//�������չ������߳�
	pthread_join(pool->managerID, NULL);

	//�����������������߳�
	for (int i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	}
	//�ͷŶ��ڴ�
	if (pool->taskQ) {
		free(pool->taskQ);
	}
	if (pool->threadIDs) {
		free(pool->threadIDs);
	}
	
	//������ + ��������
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
		//�����������߳�
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	//�ж��̳߳��Ƿ񱻹ر�
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	//��������
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	//�ƶ����
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;
	//��������������
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


//��������
void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		//��ǰ��������Ƿ�Ϊ��
		while (pool->queueSize == 0 && !pool->shutdown) {
			//���������߳�
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);  //�����ĸ��׶�  ���� --> ���� --->����--->����
		    
			//�ж��ǲ���Ҫ�����߳�
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

		//�ж��̳߳��Ƿ񱻹ر���
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool);
			//pthread_exit(NULL);
			threadExit(pool);
		}
		//�����������ȡ��һ������    ������
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;

		//�ƶ�ͷ���
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity; //��һ�����λ��
		pool->queueSize--;
		//����������
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		//ִ������ǰ
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		//��ʼִ������
		task.function(task.arg);
		free(task.arg);
		task.arg = NULL;

		//����ִ����ɺ�
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}


}

void* manager(void* arg)
{
	//����ְ��   �����������߳�
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		//ÿ��3s���һ��
		sleep(3);

		//ȡ���̳߳�������������͵�ǰ�̵߳�����
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;    //ʵ���������
		int liveNum = pool->liveNum;        //�̴߳�����
		pthread_mutex_unlock(&pool->mutexPool);

		//ȡ��æ�̵߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;    //ʵ���������
		pthread_mutex_unlock(&pool->mutexBusy);

		//�����߳�    ����ĸ���>�����̸߳���  && �����߳���<����߳���
		if (queueSize > liveNum && liveNum < pool->maxNum) {
			int counter = 0;
			pthread_mutex_lock(&pool->mutexPool);
			//��0��ʼ��һ������λ��
			for (int i = 0; i < pool->maxNum && counter < NUMBER&&pool->liveNum<pool->maxNum; ++i) {
				if (pool->threadIDs[i] == 0) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
				
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		//�����߳�     æ���߳�*2 < �����߳� && �����߳�>��С�߳���
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {

			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			//�ù������߳���ɱ
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