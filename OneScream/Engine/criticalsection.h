/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ULIB_CRITICALSECTION_H__
#define __ULIB_CRITICALSECTION_H__

#if defined(WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "isync.h"

#ifdef _DEBUG
#define CS_TRACK_OWNER 1
#endif  // _DEBUG

#ifdef CS_TRACK_OWNER
#define TRACK_OWNER(x) x
#else  // !CS_TRACK_OWNER
#define TRACK_OWNER(x)
#endif  // !CS_TRACK_OWNER

namespace Sync {
	
#if defined(WIN32)
class CriticalSection : public ISync {
public:
  CriticalSection() {
    InitializeCriticalSection(&crit_);
    // Windows docs say 0 is not a valid thread id
    TRACK_OWNER(thread_ = 0);
  }
  ~CriticalSection() {
    DeleteCriticalSection(&crit_);
  }
  void Enter() {
    EnterCriticalSection(&crit_);
    TRACK_OWNER(thread_ = GetCurrentThreadId());
  }
  bool TryEnter() {
    if (TryEnterCriticalSection(&crit_) != FALSE) {
      TRACK_OWNER(thread_ = GetCurrentThreadId());
      return true;
    }
    return false;
  }
  void Leave() {
    TRACK_OWNER(thread_ = 0);
    LeaveCriticalSection(&crit_);
  }

  virtual void Lock() {Enter();}
  virtual void UnLock() {Leave();}
  virtual bool TryLock() {return TryEnter();}

#if CS_TRACK_OWNER
  bool CurrentThreadIsOwner() const { return thread_ == GetCurrentThreadId(); }
#endif  // CS_TRACK_OWNER

private:
  CRITICAL_SECTION crit_;
  TRACK_OWNER(DWORD thread_);  // The section's owning thread id
};
#elif defined(__APPLE__)
class CriticalSection : public ISync {
public:
  CriticalSection() {
    pthread_mutexattr_t mutex_attribute;
    pthread_mutexattr_init(&mutex_attribute);
    pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &mutex_attribute);
    pthread_mutexattr_destroy(&mutex_attribute);

    TRACK_OWNER(thread_ = 0);
  }
  ~CriticalSection() {
    pthread_mutex_destroy(&mutex_);
  }
  void Enter() {
    pthread_mutex_lock(&mutex_);
    TRACK_OWNER(thread_ = pthread_self());
  }
  bool TryEnter() {
    if (pthread_mutex_trylock(&mutex_) == 0) {
      TRACK_OWNER(thread_ = pthread_self());
      return true;
    }
    return false;
  }
  void Leave() {
    TRACK_OWNER(thread_ = 0);
    pthread_mutex_unlock(&mutex_);
  }

  virtual void Lock() {Enter();}
  virtual void UnLock() {Leave();}
  virtual bool TryLock() {return TryEnter();}

#if CS_TRACK_OWNER
  bool CurrentThreadIsOwner() const { return pthread_equal(thread_, pthread_self()); }
#endif  // CS_TRACK_OWNER

private:
  pthread_mutex_t mutex_;
  TRACK_OWNER(pthread_t thread_);
};
#endif

} // namespace sync

// TODO: Replace with platform-specific "atomic" ops.
// Something like: google3/base/atomicops.h TODO: And, move
// it to atomicops.h, which can't be done easily because of complex
// compile rules.
class AtomicOps {
public:
	static long	SafeGet(long& nVal)
	{
		#if defined(WIN32)
		long nSafeVal = InterlockedExchange(&nVal, nVal);
		#elif defined(__APPLE__)
		long nSafeVal = __sync_lock_test_and_set(&nVal, nVal);
		#else
		#error "please define platform!"
		#endif

		return nSafeVal;
	}

	static long	SafeSet(long& nVal, long nNewVal)
	{
		long nSafeVal;
		#if defined(WIN32)
		nSafeVal = InterlockedExchange(&nVal, nNewVal);
		#elif defined(__APPLE__)
		nSafeVal = __sync_lock_test_and_set(&nVal, nNewVal);
		#else
		#error "please define platform!"
		#endif

		return nSafeVal;
	}

public:
#ifdef WIN32
	// Assumes sizeof(int) == sizeof(LONG), which it is on Win32 and Win64.
	static int Increment(int* i) {
		return ::InterlockedIncrement(reinterpret_cast<LONG*>(i));
	}
	static int Decrement(int* i) {
		return ::InterlockedDecrement(reinterpret_cast<LONG*>(i));
	}
#else
	static int Increment(int* i) {
		// Could be faster, and less readable:
		// static CriticalSection* crit = StaticCrit();
		// SafeLock scope(crit);
		SafeLock scope(StaticCrit());
		return ++(*i);
	}

	static int Decrement(int* i) {
		// Could be faster, and less readable:
		// static CriticalSection* crit = StaticCrit();
		// SafeLock scope(crit);
		SafeLock scope(StaticCrit());
		return --(*i);
	}

private:
	static Sync::CriticalSection* StaticCrit() {
		static Sync::CriticalSection* crit = new Sync::CriticalSection();
		return crit;
	}
#endif
};

#endif // __ULIB_CRITICALSECTION_H__
