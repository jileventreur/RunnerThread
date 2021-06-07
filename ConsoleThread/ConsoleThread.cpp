// ConsoleThread.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include <future>
#include <functional>
#include <condition_variable>
#include <windows.h>
#include <atomic>

using namespace std;

class PythonInstance
{
public:
	void func1() { std::cout << "PythonInstance : func1\n"; }
	int func2(int i) { std::cout << "PythonInstance : func2 : " << i << '\n'; return ++i; }
};

using lambdaType = std::function<int(PythonInstance)>;

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
		cout << "threadproc " << GetCurrentThreadId() << endl;		
		//while (wantQuitExec == false) {
		std::unique_lock<std::mutex> lk(execOrQuitMutex);
		while (wantQuit == false)
		{
			execOrQuitCv.wait(lk, [this] {return readyToExec || wantQuit; });
			if (readyToExec)
			{
				readyToExec = false;
				lambda(python);
				{
					std::lock_guard<std::mutex> lockReady(execDoneMutex);
					execDone = true;
				}
				execDoneCV.notify_one();
			}
		}
		cout << "end of threadproc\n";
	}

	void printThreadId() { cout << "printCurrentThreadId : " << GetCurrentThreadId() << endl; }
	
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

//template <typename Functor, typename ... Args>
//auto runPythonfFunction(PythonInstance &p, Functor funcptr, Args &&... args)
//{
//	using returnType = std::result_of_t<Functor(PythonInstance, Args...)>();
//
//	return p.*funcptr(std::forward<Args>(args)...);
//}

//TODO template this function
int runPythonFunction(PythonRunner &runner, int(PythonInstance::*funcptr)(int), int arg1)
{
	int ret{};
	auto lambda = [&](PythonInstance p)
	{
		ret = (p.*funcptr)(arg1);
		return 0;
	};
	runner.queryExecLambda(lambda);
	return ret;
}

int main()
{
	cout << "Main " << GetCurrentThreadId() << endl;
	{
		PythonRunner t;
		//std::thread t1(&Toto::Exec,&t);	
		//std::thread t1(&Toto::Other, &t);
		//auto lambda1 = [&t](PythonInstance p)
		//{
		//	t.printThreadId();
		//	p.func1();
		//	return 0;
		//};
		//int test = 0;
		//std::cout << "test : " << test << '\n';
		//auto lambda2 = [&t, &test](PythonInstance p)
		//{
		//	t.printThreadId();
		//	test = p.func2(1);
		//	return 0; 
		//};

		//t.queryExecLambda(lambda1);
		const auto test = runPythonFunction(t, &PythonInstance::func2, 0);
		std::thread t1([&] {runPythonFunction(t, &PythonInstance::func2, 0); });
		t1.join();
		std::cout << "test : " << test << '\n';
	}
	//t1.join();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
