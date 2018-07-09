
#include "OSMutex.h"
#include "OSThread.h"

OSMutex::OSMutex(BOOL bTrace/* = false*/)
 :  fHolder(0),
	fTrace(bTrace),
	fHolderCount(0)
{
	::InitializeCriticalSection(&fMutex);
}

OSMutex::~OSMutex()
{
	::DeleteCriticalSection(&fMutex);
}

void OSMutex::RecursiveLock(const char * lpFile, int nLine)
{
	//
	// We already have this mutex. Just refcount and return
	OSThread * lpThread = OSThread::GetCurrent();
	if (::GetCurrentThreadId() == fHolder)
	{
		fHolderCount++;
		(fTrace) ? blog(LOG_INFO, "%s(%lu), [Lock] %lu, %lu\n", lpFile, nLine, ::GetCurrentThreadId(), fHolderCount+1) : NULL;
		return;
	}
	(fTrace) ? blog(LOG_INFO, "%s(%lu), [Lock] %lu, %lu\n", lpFile, nLine, ::GetCurrentThreadId(), fHolderCount+1) : NULL;
	::EnterCriticalSection(&fMutex);
	ASSERT(fHolder == 0);
	fHolder = ::GetCurrentThreadId();
	fHolderCount++;
	ASSERT(fHolderCount == 1);
}

void OSMutex::RecursiveUnlock()
{
	if (::GetCurrentThreadId() != fHolder)
		return;
	(fTrace) ? blog(LOG_INFO, "[UnLock] %lu, %lu\n", fHolder, fHolderCount-1) : NULL;
	ASSERT(fHolderCount > 0);
	fHolderCount--;
	if (fHolderCount == 0)
	{
		fHolder = 0;
		::LeaveCriticalSection(&fMutex);
	}
}

Bool16 OSMutex::RecursiveTryLock()
{
	//
	// We already have this mutex. Just refcount and return
	if (::GetCurrentThreadId() == fHolder)
	{
		fHolderCount++;
		return true;
	}

	Bool16 theErr = (Bool16)::TryEnterCriticalSection(&fMutex); // Return values of this function match our API
	if (!theErr)
		return theErr;
	ASSERT(fHolder == 0);
	fHolder = ::GetCurrentThreadId();
	fHolderCount++;
	ASSERT(fHolderCount == 1);
	return true;
}
