#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <functional>
#include <chrono>

class threadpool {
public:
	// replicate Java's Runnable interface, this will do the actual work
	class Runnable {
		public:
			Runnable(){}
			~Runnable(){}
			virtual void run() = 0;
	};

	threadpool(int num_threads);
	~threadpool();
	void execute(Runnable job);
	void terminate();

private:
	std::queue<Runnable> shared_queue;
	std::vector<std::thread> threads;
	bool is_stop;
	std::mutex mutex;
	std::condition_variable condition;
};


threadpool::threadpool(int num_threads) {
	std::function<void()> handler = [&]() {
		while (!is_stop) {
			std::unique_lock<std::mutex> lock(mutex);

			if (!shared_queue.empty()) {
				Runnable value;
				job = shared_queue.front();
				shared_queue.pop();

				lock.unlock();
				condition.notify_one();

				(job.run)();
				std::this_thread::yield();
			} else {
				condition.wait(lock, [&]{ return (!shared_queue.empty()) || is_stop; });
				if (is_stop) {
					lock.unlock();
					break;
				}
			}
		}
	};

	is_stop = false;
	for (int i = 0; i < num_threads; i++) threads.push_back(std::thread(handler));
}

void threadpool::execute(Runnable job) {
	// critical section
	{
	  std::unique_lock<std::mutex> lock(mutex);
	  if (!is_stop) shared_queue.push(job);
	}
	condition.notify_one();
}

threadpool::~threadpool() {
	terminate();
	for (std::thread &t: threads) {
		if (t.joinable()) t.join();
	}
}

void threadpool::terminate() {
	// critical section
	{
		std::unique_lock<std::mutex> lock(mutex);
		is_stop = true;
	}
	condition.notify_all();
}
