// ConsoleThread.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include <future>
#include <functional>
#include <condition_variable>
#include <windows.h>
#include <atomic>

class PythonInstance
{
public:
	void func1() { std::cout << "PythonInstance : func1\n"; }
	int func2(int i) { std::cout << "PythonInstance : func2 : " << i << '\n'; return ++i; }
};

using lambdaType = std::function<void(PythonInstance &)>;

class PythonRunner {
	std::thread mt;
	
	std::mutex execOrQuitMutex;
	std::condition_variable execOrQuitCv;
	bool readyToExec = false;
	bool wantQuit = false;
	lambdaType lambda;
	
	std::mutex execDoneMutex;
	std::condition_variable execDoneCV;
	bool execDone = false;

	std::mutex otherMutex;
public:
	
	PythonRunner() {
		mt = std::thread(&PythonRunner::threadproc, this);
	}
	
	~PythonRunner()
	{
		std::cout << "Destructor\n";
		{
			std::lock_guard<std::mutex> lockReady(execOrQuitMutex);
			wantQuit = true;
		}
		execOrQuitCv.notify_one();
		mt.join();
	}

	void threadproc()
	{
		PythonInstance python;
		std::cout << "threadproc " << GetCurrentThreadId() << '\n';		
		std::unique_lock<std::mutex> lk(execOrQuitMutex);
		while (wantQuit == false)
		{
			execOrQuitCv.wait(lk, [this] {return readyToExec || wantQuit; });
			if (readyToExec)
			{
				readyToExec = false;
					std::cout << "threadproc: lambda call in thread " << GetCurrentThreadId() << '\n';
				lambda(python);
				{
					std::lock_guard<std::mutex> lockReady(execDoneMutex);
					execDone = true;
				}
				execDoneCV.notify_one();
			}
		}
		std::cout << "end of threadproc\n";
	}

	void printThreadId() { std::cout << "printCurrentThreadId : " << GetCurrentThreadId() << '\n'; }
	
	void queryExecLambda(const lambdaType l) {
		std::lock_guard<std::mutex> otherLock(otherMutex);
		std::cout << "Other called in " << GetCurrentThreadId() << '\n';
		lambda = l;
		{
			std::lock_guard<std::mutex> lockReady(execOrQuitMutex);
			readyToExec = true;
		}
		execOrQuitCv.notify_one();
		std::unique_lock<std::mutex> lk(execDoneMutex);
		execDoneCV.wait(lk, [this] {return execDone; });
		execDone = false;
	}
};

template <
	typename FuncPtr, typename ... Args,
	typename returnType = std::result_of_t<FuncPtr(PythonInstance, Args...)>,
	typename = std::enable_if_t<!std::is_same<void, returnType>::value>
>
auto runPythonFunction(PythonRunner &runner, FuncPtr funcptr, Args &&... args)
{
	returnType ret{};
	const auto lambda = [&] (PythonInstance &p)
	{
		ret = (p.*funcptr)(std::forward<Args>(args)...);
	};
	runner.queryExecLambda(lambda);

	return ret;
}

template <
	typename FuncPtr, typename ... Args,
	typename returnType = std::result_of_t<FuncPtr(PythonInstance, Args...)>,
	typename = std::enable_if_t<std::is_same<void, returnType>::value>
	>
void runPythonFunction(PythonRunner &runner, FuncPtr funcptr, Args &&... args)
{
	const auto lambda = [&](PythonInstance &p)
	{
		(p.*funcptr)(std::forward<Args>(args)...);
	};
	runner.queryExecLambda(lambda);
}

int main()
{
	std::cout << "Main " << GetCurrentThreadId() << '\n';
	PythonRunner t;
	runPythonFunction(t, &PythonInstance::func1);
	//runPythonFunction(t, &PythonInstance::func2); // <- Do not compile (bad args)
	std::thread t1([&] {runPythonFunction(t, &PythonInstance::func2, 0); });
	t1.join();
}