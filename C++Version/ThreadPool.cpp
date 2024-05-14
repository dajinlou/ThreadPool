#include "ThreadPool.h"

#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>
#include <pthread.h>

using namespace std;

const int NUMBER = 2;

ThreadPool::ThreadPool(int min, int max):shutdown(false)
{
    do
    {
        //1.实例化任务队列
        taskQ = new TaskQueue;
        if (taskQ == NULL)
        {
            cout << "new taskQ fail ..." << endl;
            break;
        }

        threadIDs = new pthread_t[max];
        if (nullptr == threadIDs)
        {
            cout << "new threadIDs fail ..." << endl;
            break;
        }
        memset(threadIDs, 0, sizeof(pthread_t) * max);
        minNum = min;
        maxNum = max;
        busyNum = 0;
        liveNum = min; 
        exitNum = 0;
        if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
            pthread_cond_init(&notEmpty, NULL) != 0) {
            cout << "mutex or condition init fail.." << endl;
            break;
        }

        shutdown = false;

        //创建线程
        pthread_create(&managerID, NULL, manager, this);
        for (int i = 0; i < min; ++i) {
            pthread_create(&threadIDs[i], NULL, worker, this);
            pthread_detach(threadIDs[i]);
        }

        

        return;
    } while (false);

    //释放资源
    if (threadIDs) {
        delete[] threadIDs;
    }
    if (taskQ != NULL) {
        delete taskQ;
    }

}

ThreadPool::~ThreadPool()
{
    shutdown = true;
    //阻塞回收管理者线程
    pthread_join(managerID, NULL);
    //唤醒阻塞的消费者线程
    for (int i = 0; i < liveNum; ++i)
    {
        pthread_cond_signal(&notEmpty);
    }
    if (taskQ != NULL) {
        delete taskQ;
    }
    if (threadIDs != NULL) {
        delete[] threadIDs;
    }

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);

}

void ThreadPool::addTask(Task task)
{
    if (this->shutdown) {     
        return;
    }
    //添加任务
    taskQ->addTask(task);
    pthread_cond_signal(&notEmpty);
}

int ThreadPool::getBusyNum()
{
    int busyNum = 0;
    pthread_mutex_lock(&mutexPool);
    busyNum = this->busyNum;
    pthread_mutex_unlock(&mutexPool);

    return busyNum;
}

int ThreadPool::getAliveNum()
{
    int aliveNum = 0;
    pthread_mutex_lock(&mutexPool);
    aliveNum = this->liveNum;
    pthread_mutex_unlock(&mutexPool);
    return aliveNum;
}

void* ThreadPool::worker(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (true)
    {
        pthread_mutex_lock(&pool->mutexPool);
        //当前任务队列是否为空
        while (pool->taskQ->taskNumber()== 0 && !pool->shutdown)
        {
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
            //判断是不是要销毁线程
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                }
            }
        }
        //判断线程池是否被关闭了
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }

        pool->busyNum++;
        //从任务中取出一个任务
        Task task = pool->taskQ->takeTask();
        pthread_mutex_unlock(&pool->mutexPool);
        cout << "thread" << to_string(pthread_self()) << " start working ..." << endl;
        
        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;
        cout << "thread" << to_string(pthread_self()) << " end working ..." << endl;

        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexPool);

    }

    return nullptr;
}

void* ThreadPool::manager(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (!pool->shutdown) {
        //每隔3s检测一次
        sleep(3);

        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->taskQ->taskNumber();
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexPool);

        //添加线程
        //  workNum>liveNum   && workNum < maxNum
        if (queueSize > liveNum && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i)
            {
                if (pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    pthread_detach(pool->threadIDs[i]);
                    counter++;
                    pool->liveNum++;
                }
            }

            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁线程
        // 忙的线程*2 < 存活的线程数 && 存活的线程 > 最小线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum && !pool->shutdown)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //让工作的线程自杀
            for (int i = 0; i < NUMBER && !pool->shutdown; ++i)
            {
                pthread_cond_signal(&pool->notEmpty);
            }

        }
    }
    cout << "manger die---------------------------------------------------------" << endl;
    return nullptr;
}

void ThreadPool::threadExit()
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < maxNum; ++i) {
        if (threadIDs[i] == tid) {
            threadIDs[i] = 0;
            cout << "threadExit() called, " << to_string(tid) << " exiting ...." << endl;
            break;
        }
    }
    pthread_exit(NULL);
}
