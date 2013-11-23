// PrimeTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

//#define _HAS_ITERATOR_DEBUGGING 0

#include <vector>
#include <iostream>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <Windows.h>

// Utilities

int intSqrt(int n)
{
	return static_cast<int>(sqrt(static_cast<double>(n)));
}

class Timer
{
public:
	Timer(char *msg_):msg(msg_), start(GetTickCount()) { }
	~Timer() { std::cout << msg << ": " << GetTickCount() - start << "\n"; }
private:
	DWORD start;
	const char *msg;
};

// First solution

bool IsPrime(int n)
{
#ifdef TEST_SLOW_CONDITION
	Sleep(1);
#endif
	int sqrt_n = intSqrt(n);
	for (int i = 2; i <= sqrt_n; ++i)
	{
		if (n % i == 0)
			return false;
	}
	return true;
}

int FindPrimes_v1(int max)
{
	std::vector<int> result;
	int count = 0;
	for (int n = 2; n <= max; ++n)
	{
		if (IsPrime(n))
		{
			result.push_back(n);
		}
	}
	
	return result.size();
}

// First solution in multiple threads

const int cpuCount = 8;
size_t minList = 10 * 1000;
size_t maxList = 20 * 1000;

std::mutex m;
std::condition_variable cv_needMore, cv_handleNow;
int count = 0;
std::vector<int> vector;
std::vector<int> result_array[cpuCount];
int time_array[cpuCount];
int endNumber;
bool done;

std::thread *threads;

void FindPrimes_Thread(int threadIndex)
{
	while (1)
	{
		int n;
		{
			std::unique_lock<std::mutex> lock(m);
			cv_handleNow.wait(lock, []{return !vector.empty() || done;});
			if (vector.empty() && done)
			{
				return;
			}
			n = vector.back();
			vector.pop_back();
			if (vector.size() <= minList)
			{
				cv_needMore.notify_one();
			}
		}

		DWORD t = GetTickCount();
		if (IsPrime(n))
		{
			result_array[threadIndex].push_back(n);
		}
		time_array[threadIndex] += GetTickCount() - t;
	}
}

int FindPrimes_v2(int max)
{
	done = false;
	threads = new std::thread[cpuCount];

	{
	Timer t("queue");

	for (int n = 2; n <= max; ++n)
	{
//		std::unique_lock<std::mutex> lock(m);
		m.lock();
//		cv_needMore.wait(lock, []{ return vector.size() < maxList;});
		vector.push_back(n);
		if (vector.size() >= minList)
		{
//			cv_handleNow.notify_one();
		}
		m.unlock();
	}

	}

	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i] = std::thread(FindPrimes_Thread, i);
	}
	{
		std::lock_guard<std::mutex> lock(m);
		done = true;
	}
	cv_handleNow.notify_all();
	
	int total = 0;
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i].join();
		std::cout << "    count: " << result_array[i].size() << "\t";
		std::cout << "    time: " << time_array[i] << "\n";
		total += result_array[i].size();

		result_array[i].clear();
		time_array[i] = 0;
	}
	delete [] threads;

	return total;
}

// First solution in multiple threads #2

void FindPrimes_Thread_v3(int threadIndex)
{
	for (int n = 2; n <= endNumber; ++n)
	{
		if ((n - 2) / maxList % cpuCount != threadIndex)
			continue;

		if (IsPrime(n))
		{
			result_array[threadIndex].push_back(n);
		}
	}
}

int FindPrimes_v3(int max)
{
	endNumber = max;
	threads = new std::thread[cpuCount];
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i] = std::thread(FindPrimes_Thread_v3, i);
	}

	int total = 0;
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i].join();
		total += result_array[i].size();

		result_array[i].clear();
		time_array[i] = 0;
	}
	delete [] threads;

	return total;
}

// Different solutoin

int FindPrimes_v4(int max)
{
	std::vector<int> result;

	bool isPrime;
	for (int n = 2; n <= max; ++n)
	{
		DWORD t = GetTickCount();
		isPrime = true;
		int sqrt_n = intSqrt(n);
		for (auto it = begin(result); it != end(result) && *it <= sqrt_n; ++it)
		{
			if (n % *it == 0)
			{
				isPrime = false;
				break;
			}
		}
		if (isPrime)
		{
			result.push_back(n);
		}
	}

	return result.size();
}

// Different solution in multiple threads

std::vector<int> primes;

void FindPrimes_Base_v5(int max)
{
	primes.clear();

	bool isPrime;
	for (int n = 2; n <= max; ++n)
	{
		isPrime = true;
		int sqrt_n = intSqrt(n);
		for (auto it = begin(primes); it != end(primes) && *it <= sqrt_n; ++it)
		{
			if (n % *it == 0)
			{
				isPrime = false;
				break;
			}
		}
		if (isPrime)
		{
			primes.push_back(n);
		}
	}
}

bool IsPrime_withBase_v5(int n)
{
	int sqrt_n = intSqrt(n);
	for (auto it = begin(primes); it != end(primes) && *it <= sqrt_n; ++it)
	{
		if (n % *it == 0)
		{
			return false;
		}
	}
	return true;
}

void FindPrimes_Thread_Block_v5(int blockCount, int saveIndex, int blockIndex)
{
	int start_ = (endNumber / blockCount * blockIndex) + 1;
	int end_ = endNumber / blockCount * (blockIndex + 1);
	for (int n = max(2, start_); n <= end_; ++n)
	{
		if (IsPrime_withBase_v5(n))
		{
			result_array[saveIndex].push_back(n);
		}
	}
}

void FindPrimes_Thread_v5(int threadIndex)
{
	int blockCount = cpuCount * 2;
	FindPrimes_Thread_Block_v5(blockCount, threadIndex, threadIndex);
	FindPrimes_Thread_Block_v5(blockCount, threadIndex, (blockCount - 1) - threadIndex);
}

int FindPrimes_v5(int max)
{
	FindPrimes_Base_v5(intSqrt(max));
	endNumber = max;

	threads = new std::thread[cpuCount];
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i] = std::thread(FindPrimes_Thread_v5, i);
	}

	int total = 0;
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i].join();
		total += result_array[i].size();

		result_array[i].clear();
		time_array[i] = 0;
	}

	primes.clear();
	delete [] threads;

	return total;
}

//

bool IsPrime_Different(int n)
{
	int sqrt_n = intSqrt(n);
	for (auto it = begin(primes); it != end(primes) && *it <= sqrt_n; ++it)
	{
		if (n % *it == 0)
		{
			return false;
		}
	}
	return true;
}

void FindPrimes_Thread_v6(int threadIndex)
{
	while (1)
	{
		int n;
		{
			{
				std::unique_lock<std::mutex> lock(m);
				cv_handleNow.wait(lock, []{return !vector.empty() || done;});
				if (vector.empty() && done)
				{
					return;
				}
				n = vector.back();
				vector.pop_back();
			}
			if (vector.size() <= minList)
			{
				cv_needMore.notify_one();
			}
		}

		DWORD t = GetTickCount();
		if (IsPrime_Different(n))
		{
			result_array[threadIndex].push_back(n);
		}
		time_array[threadIndex] += GetTickCount() - t;
	}
}

int FindPrimes_v6(int max)
{
	FindPrimes_Base_v5(intSqrt(max));

	done = false;
	threads = new std::thread[cpuCount];
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i] = std::thread(FindPrimes_Thread_v6, i);
	}

	for (int n = 2; n <= max; ++n)
	{
		std::unique_lock<std::mutex> lock(m);
		cv_needMore.wait(lock, []{return vector.size() < maxList;});
		vector.push_back(n);
		if (vector.size() >= minList)
		{
			cv_handleNow.notify_one();
		}
	}
	{
		std::lock_guard<std::mutex> lock(m);
		done = true;
	}
	cv_handleNow.notify_all();

	int total = 0;
	for (int i = 0; i < cpuCount; ++i)
	{
		threads[i].join();
		std::cout << "    count: " << result_array[i].size() << "\t";
		std::cout << "    time: " << time_array[i] << "\n";
		total += result_array[i].size();

		result_array[i].clear();
		time_array[i] = 0;
	}
	primes.clear();
	delete [] threads;

	return total;
}

// main

int main()
{
	int MAX,  COUNT;

	MAX = 10 * 1000 * 1000;	COUNT = 664579;
//	{ Timer t("10 million - single");            if (FindPrimes_v1(MAX) != COUNT) DebugBreak(); }
	{ Timer t("10 million - multi");             if (FindPrimes_v2(MAX) != COUNT) DebugBreak(); }
//	{ Timer t("10 million - no lock");           if (FindPrimes_v3(MAX) != COUNT) DebugBreak(); }
//	{ Timer t("10 million - use list");          if (FindPrimes_v4(MAX) != COUNT) DebugBreak(); }
//	{ Timer t("10 million - use list no lock");  if (FindPrimes_v5(MAX) != COUNT) DebugBreak(); }
	{ Timer t("10 million - use list multi");    if (FindPrimes_v6(MAX) != COUNT) DebugBreak(); }//
	std::cout << "\n";

	MAX = 100 * 1000 * 1000; COUNT = 5761455;
	{ Timer t("100 million - single");           if (FindPrimes_v1(MAX) != COUNT) DebugBreak(); }
	{ Timer t("100 million - multi");            if (FindPrimes_v2(MAX) != COUNT) DebugBreak(); }
	{ Timer t("100 million - no lock");          if (FindPrimes_v3(MAX) != COUNT) DebugBreak(); }
	{ Timer t("100 million - use list");         if (FindPrimes_v4(MAX) != COUNT) DebugBreak(); }
	{ Timer t("100 million - use list no lock"); if (FindPrimes_v5(MAX) != COUNT) DebugBreak(); }
	{ Timer t("100 million - use list multi");   if (FindPrimes_v6(MAX) != COUNT) DebugBreak(); }
	std::cout << "\n";

	MAX = 1000 * 1000 * 1000; COUNT = 50847534;
	{ Timer t("1 billion - single");             if (FindPrimes_v1(MAX) != COUNT) DebugBreak(); }
	{ Timer t("1 billion - multi");              if (FindPrimes_v2(MAX) != COUNT) DebugBreak(); }
	{ Timer t("1 billion - no lock");            if (FindPrimes_v3(MAX) != COUNT) DebugBreak(); }
	{ Timer t("1 billion - use list");           if (FindPrimes_v4(MAX) != COUNT) DebugBreak(); }
	{ Timer t("1 billion - use list no lock");   if (FindPrimes_v5(MAX) != COUNT) DebugBreak(); }
	{ Timer t("1 billion - use list multi");     if (FindPrimes_v6(MAX) != COUNT) DebugBreak(); }
	std::cout << "\n";

	return 0;
}
