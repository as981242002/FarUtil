//共享栈非对称协程
#ifndef FAR_COROUTINE_H
#define FAR_COROUTINE_H

#include<functional>
#include<thread>
#include<future>
#include<vector>
#include<list>
#include<string>
 
#include<cstdint>
#include<cstring>
#include<cstdio>
#include<cassert>

#ifndef STACK_LIMIT
#define STACK_LIMIT (1024 * 1024)
#endif // !STACK_LIMIT



#if _MSC_VER
#include<Windows.h>
#endif // _MSC_VER

using ::std::string;
using ::std::wstring;

namespace coroutine
{
using Routine_t = unsigned int;
using Func = std::function<void()>;
#if _MSC_VER
struct Routine
{
	Func func;
	bool finished;
	LPVOID fiber;

	Routine(Func f): func(f), finished(false),fiber(nullptr) {}
	~Routine()
	{
		DeleteFiber(fiber);
	}
};

struct Ordinator
{
	std::vector<Routine*> routines; 
	std::list<Routine_t> indexs;
	Routine_t current;
	size_t stack_size;
	LPVOID fiber;

	Ordinator(size_t ss = STACK_LIMIT): current(0), stack_size(ss) 
	{
		fiber = ConvertThreadToFiber(nullptr);
	}
	
	~Ordinator()
	{
		for (auto& routine : routines)
		{
			delete routine;
		}
	}
};

thread_local static Ordinator ordinator; //调度协程

inline Routine_t create(Func f)
{
	Routine* routine = new Routine(f);
	if (ordinator.indexs.empty())
	{
		ordinator.routines.push_back(routine);
		return ordinator.routines.size();
	}
	else
	{
		Routine_t id = ordinator.indexs.front();
		ordinator.indexs.pop_front();
		assert(ordinator.routines[id -1] == nullptr);
		ordinator.routines[id - 1] = routine;
		return id;
	}
}

inline void destroy(Routine_t id)
{
	Routine* routine = ordinator.routines[id - 1];
	assert(routine != nullptr);

	delete routine;
	
	ordinator.routines[id - 1] = nullptr;
	ordinator.indexs.push_back(id);
}

inline void __stdcall entry(LPVOID lpParameter)
{
	Routine_t id = ordinator.current;
	Routine* routine = ordinator.routines[id - 1];
	assert(routine != nullptr);
	
	routine->func();
	routine->finished = true;
	ordinator.current = 0;

	SwitchToFiber(ordinator.fiber);
}
#endif
}


#endif // !FAR_COROUTINE_H
