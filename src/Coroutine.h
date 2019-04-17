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

	Routine(Func f) : func(f), finished(false), fiber(nullptr) {}
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

	Ordinator(size_t ss = STACK_LIMIT) : current(0), stack_size(ss)
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
		assert(ordinator.routines[id - 1] == nullptr);
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

inline int resume(Routine_t id)
{
	assert(ordinator.current == 0);

	Routine* routine = ordinator.routines[id - 1];
	if (routine == nullptr)
		return -1;

	if (routine->finished)
		return -2;
	if (routine->fiber == nullptr)
	{
		routine->fiber = CreateFiber(ordinator.stack_size, entry, 0);
		ordinator.current = id;
		SwitchToFiber(routine->fiber);
	}
	else
	{
		ordinator.current = id;
		SwitchToFiber(routine->fiber);
	}

	return 0;
}

//挂起当前的协程 返回到调度协程
inline void yield()
{
	Routine_t id = ordinator.current;
	Routine* routine = ordinator.routines[id - 1];

	assert(routine != nullptr);

	ordinator.current = 0;
	SwitchToFiber(ordinator.fiber); //回到调度协程
}

inline Routine_t current()
{
	return ordinator.current;
}

template<typename Function, typename ...Args>
inline decltype(auto) await(Function&& func, Args&& ... args)
{
	auto future = std::async(std::launch::async, func, std::forward<Args>(args)...);  //以任务方式开始
	std::future_status status = future.wait_for(std::chrono::milliseconds(0));

	while (status == std::future_status::timeout) //如果是超时的状态
	{
		if (ordinator.current != 0)
			yield(); //挂起

		status = future.wait_for(std::chrono::milliseconds(0));
	}

	return future.get();
}

#endif

template<typename Type>
class Channel
{
public:
	Channel() :routine_(0)
	{
	}
	Channel(Routine_t id) :routine_(id)
	{
	}


	inline void consumer(Routine_t id)
	{
		routine_ = id;
	}

	inline void push(const Type& obj)
	{
		list_.push_back(obj);
		if (routine_ && routine_ != current())
		{
			resume(routine_);
		}

	}

	inline Type pop()
	{
		if (!routine_)
			routine_ = current();
		while (list_.empty())
		{
			yield();
		}

		Type obj = std::move(list_.front());
		list_.pop_front();
		return std::move(obj);
	}

	inline void clear()
	{
		list_.clear();
	}

	inline void touch()
	{
		if (routine_ && routine_ != current())
		{
			resume(routine_);
		}
	}

	inline size_t size()
	{
		return list_.size();
	}

	inline bool empty()
	{
		return list_.empty();
	}

private:
	std::list<Type> list_;
	Routine_t routine_;
};

}


#endif // !FAR_COROUTINE_H
