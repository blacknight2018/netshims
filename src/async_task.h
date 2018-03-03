#ifndef ASYNC_TASK_H_
#define ASYNC_TASK_H_
/**
*      async_task.h/async_task.cpp
*      实现异步任务调度
*/

#include <Windows.h>
#include "ref.h"
using namespace ref;

class AsyncTask : public AutoSink
{
	friend class AsyncTaskPool;
public:
	AsyncTask():m_work(NULL), m_timer(NULL){}
	virtual ~AsyncTask() {}
public:
	virtual void OnCalling(void) = 0;

private:
	PTP_WORK m_work;
	PTP_TIMER m_timer;
	DWORD m_period;

	void Bind(PTP_WORK work = NULL, PTP_TIMER tm = NULL,DWORD period = 0) {
		m_work = work;
		m_timer = tm;
		m_period = period;
	}
	PTP_WORK GetWork(void) { return m_work; }
	PTP_TIMER GetTimer(void) { return m_timer; }
	DWORD GetPeriod(void) { return m_period; }
};

class AsyncTaskPool : public AutoSink
{
public:
	AsyncTaskPool(int threadMinimum,int threadMaximum);
	~AsyncTaskPool();

public:

	BOOL IsSuccessed(void) { return m_success; }

	BOOL PostTask(AutoRefPtr<AsyncTask> task,int delay_ms = 0,DWORD period = 0,DWORD customAddTime_ms = 0);

	BOOL WaitTask(AutoRefPtr<AsyncTask> task);

	BOOL CancelTask(AutoRefPtr<AsyncTask> task);

private:
	PTP_POOL m_pool;
	TP_CALLBACK_ENVIRON m_callbackEnviron;
	PTP_CLEANUP_GROUP m_cleanupGroup;
	BOOL m_success;

private:
	void static CALLBACK TaskWorkCallback(PTP_CALLBACK_INSTANCE instance,PVOID context,PTP_WORK work);

	void static CALLBACK TaskAsyncCallback(PTP_CALLBACK_INSTANCE instance, PVOID context);

	void static CALLBACK TaskTimerCallback(PTP_CALLBACK_INSTANCE instance, PVOID context,PTP_TIMER timer);

protected:
	IMPLEMENT_NS_REFCOUNTING(AsyncTaskPool)
};
#endif



