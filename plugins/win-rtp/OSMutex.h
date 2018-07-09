
#pragma once

#include "HYDefine.h"

class OSMutex
{
public:
	OSMutex(BOOL bTrace = false);
	~OSMutex();

	void		Lock(const char * lpFile, int nLine)	{ this->RecursiveLock(lpFile, nLine); }
	void		Unlock()						{ this->RecursiveUnlock(); }
	UInt32		GetHoldCount()					{ return fHolderCount; }
	//	
	// Returns true on successful grab of the lock, false on failure
	Bool16		TryLock()	{ return this->RecursiveTryLock(); }
private:
	CRITICAL_SECTION	fMutex;
	DWORD				fHolder;
	UInt32				fHolderCount;
	BOOL				fTrace;
		
	void		RecursiveLock(const char * lpFile, int nLine);
	void		RecursiveUnlock();
	Bool16		RecursiveTryLock();
};

class OSMutexLocker
{
public:
	OSMutexLocker(OSMutex *inMutexP, const char * lpFile = NULL, int nLine = 0) : fMutex(inMutexP) { if (fMutex != NULL) fMutex->Lock(lpFile, nLine); }
	~OSMutexLocker()	{ if (fMutex != NULL) fMutex->Unlock(); }
	
	//void Lock() 		{ if (fMutex != NULL) fMutex->Lock(); }
	//void Unlock() 	{ if (fMutex != NULL) fMutex->Unlock(); }
private:
	OSMutex *	fMutex;
};

#define OS_MUTEX_LOCKER(A) OSMutexLocker theLock(A, __FILE__, __LINE__)
