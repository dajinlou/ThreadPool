#include "ThreadPool.h"

#include <iostream>
#include <unistd.h>

using namespace std;

void taskFunc(void* arg)
{
	int num = *(int*)arg;
	cout << "thread id:" << to_string(pthread_self()) << " working, num=" << num << endl;
	sleep(1);
}

int main()
{
	ThreadPool pool(3,10);
	for (int i = 0; i < 100; ++i) {
		int* num = new int(i+100);
		pool.addTask(Task(taskFunc, num));
	}

	sleep(30);

	return 0;
}