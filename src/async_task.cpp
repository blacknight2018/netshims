#include "async_task.h"

AsyncTaskPool::AsyncTaskPool(int threadMinimum, int threadMaximum){
	UINT rollback = 0;	//now init completed step
	m_success = FALSE;
	InitializeThreadpoolEnvironment(&m_callbackEnviron);
	if ((m_pool = CreateThreadpool(NULL)) == NULL) {
		goto jmp_cleanup;
	}
	rollback = 1;// pool creation succeeded
	SetThreadpoolThreadMaximum(m_pool, threadMaximum);
	if (SetThreadpoolThreadMinimum(m_pool, threadMinimum) == FALSE) {
		goto jmp_cleanup;
	}
	if ((m_cleanupGroup = CreateThreadpoolCleanupGroup()) == NULL) {
		goto jmp_cleanup;
	}
	rollback = 2;// Cleanup group creation succeeded
	SetThreadpoolCallbackPool(&m_callbackEnviron, m_pool);
	SetThreadpoolCallbackCleanupGroup(&m_callbackEnviron, m_cleanupGroup, NULL);
	m_success = TRUE;
	rollback = 0;
jmp_cleanup:
	switch (rollback)
	{
	case 2:
		CloseThreadpoolCleanupGroup(m_cleanupGroup);
	case 1:
		CloseThreadpool(m_pool);
		break;
	default:
		break;
	}
}


AsyncTaskPool::~AsyncTaskPool(){
	if (m_success) {
		CloseThreadpoolCleanupGroupMembers(m_cleanupGroup, FALSE, NULL);
		CloseThreadpoolCleanupGroup(m_cleanupGroup);
		DestroyThreadpoolEnvironment(&m_callbackEnviron);
		CloseThreadpool(m_pool);
	}
}

void CALLBACK AsyncTaskPool::TaskWorkCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work) {
	AutoRefPtr<AsyncTask> pTask = reinterpret_cast<AsyncTask*>(context);
	if (pTask.get()) {
		pTask->OnCalling();
		pTask->Release();
	}
	CloseThreadpoolWork(work);
}

void CALLBACK AsyncTaskPool::TaskAsyncCallback(PTP_CALLBACK_INSTANCE instance, PVOID context) {

}

void CALLBACK AsyncTaskPool::TaskTimerCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer) {
	AutoRefPtr<AsyncTask> pTask = reinterpret_cast<AsyncTask*>(context);
	if (pTask.get()) {
		pTask->OnCalling();
		if (pTask->GetPeriod()) {
			return;
		}
		pTask->Release();
	}
	CloseThreadpoolTimer(timer);
}

BOOL AsyncTaskPool::PostTask(AutoRefPtr<AsyncTask> task, int delay_ms, DWORD period, DWORD customAddTime_ms) {
	if (m_success && task.get()) {
		if (delay_ms == 0) {
			PTP_WORK work = CreateThreadpoolWork(TaskWorkCallback, (PVOID)task.get(), &m_callbackEnviron);
			if (work == NULL) return FALSE;
			task->AddRef();
			task->Bind(work, NULL,0);
			SubmitThreadpoolWork(work);
		}
		else {
			PTP_TIMER timer = CreateThreadpoolTimer(TaskTimerCallback, (PVOID)task.get(), &m_callbackEnviron);
			if (timer == NULL) {
				return FALSE;
			}
			FILETIME fileTime;
			ULARGE_INTEGER dueTime;
			dueTime.QuadPart = (ULONGLONG)-(delay_ms * 10 * 1000);
			fileTime.dwHighDateTime = dueTime.HighPart;
			fileTime.dwLowDateTime = dueTime.LowPart;
			task->AddRef();
			task->Bind(NULL, timer,period);
			SetThreadpoolTimer(timer, &fileTime, period, customAddTime_ms);
		}
		return TRUE;
	}
	return FALSE;
}

BOOL AsyncTaskPool::WaitTask(AutoRefPtr<AsyncTask> task) {
	if (m_success && task.get()) {
		if (task->GetWork()) {
			WaitForThreadpoolWorkCallbacks(task->GetWork(), FALSE);
			return TRUE;
		}
		else if(task->GetTimer()) {
			WaitForThreadpoolTimerCallbacks(task->GetTimer(), FALSE);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL AsyncTaskPool::CancelTask(AutoRefPtr<AsyncTask> task) {
	if (m_success && task.get()) {
		if (task->GetWork()) {
			WaitForThreadpoolWorkCallbacks(task->GetWork(), TRUE);
			CloseThreadpoolWork(task->GetWork());
			return TRUE;
		}
		else if (task->GetTimer()) {
			WaitForThreadpoolTimerCallbacks(task->GetTimer(), TRUE);
			CloseThreadpoolTimer(task->GetTimer());
			if (task->GetPeriod()) {
				task->Release();
			}
			return TRUE;
		}
	}
	return FALSE;
}
