/*
 * mutex.c
 *
 * Description:
 * This translation unit implements mutual exclusion (mutex) primitives.
 *
 * Pthreads-win32 - POSIX Threads Library for Win32
 * Copyright (C) 1998
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 */

/* errno.h or a replacement file is included by pthread.h */
//#include <errno.h>

#include "pthread.h"
#include "implement.h"

int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Initializes a mutex object with supplied or
      *      default attributes.
      *
      * PARAMETERS
      *      mutex
      *              pointer to an instance of pthread_mutex_t
      *      attr
      *              pointer to an instance of pthread_mutexattr_t
      *
      *
      * DESCRIPTION
      *      Initializes a mutex object with supplied or
      *      default attributes.
      *
      * RESULTS
      *              0               successfully initialized attr,
      *              EINVAL          not a valid mutex pointer,
      *              ENOMEM          insufficient memory for attr,
      *              ENOSYS          one or more requested attributes
      *                              are not supported.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  pthread_mutex_t mx;
  int oldCancelType;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  if (attr != NULL && *attr != NULL && (*attr)->pshared == PTHREAD_PROCESS_SHARED)
    {
      return ENOSYS;
    }


  /*
   * We need to prevent us from being canceled
   * unexpectedly leaving the mutex in a corrupted state.
   * We can do this by temporarily
   * setting the thread to DEFERRED cancel type
   * and resetting to the original value whenever
   * we sleep and when we return. We also check if we've
   * been canceled at the same time.
   */
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldCancelType);

  /*
   * This waits until no other thread is looking at the
   * (possibly uninitialised) mutex object, then gives
   * us exclusive access.
   */
  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);

  /*
   * We now have exclusive access to the mutex pointer
   * and structure, whether initialised or not.
   */

  mx = (pthread_mutex_t) calloc(1, sizeof(*mx));
  if (mx == NULL)
    {
      result = ENOMEM;
      goto FAIL0;
    }

  if (attr != NULL && *attr != NULL)
    {
      mx->pshared = (*attr)->pshared;
      if ((*attr)->type == PTHREAD_MUTEX_DEFAULT)
        {
          mx->type = ptw32_mutex_mapped_default;
        }
      else
        {
          mx->type = (*attr)->type;
        }
    }
  else
    {
      mx->type = ptw32_mutex_mapped_default;
      mx->pshared = PTHREAD_PROCESS_PRIVATE;
    }

  mx->lock_idx = -1;
  mx->try_lock = 0;
  mx->owner = NULL;
  mx->waiters = 0;
  mx->lastOwner = NULL;
  mx->lastWaiter = NULL;

 FAIL0:
  PTW32_OBJECT_SET(mutex, mx);

  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
    {
      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_testcancel();
    }

  return result;
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Destroys a mutex object and returns any resources
      *      to the system.
      *
      * PARAMETERS
      *      mutex
      *              pointer to an instance of pthread_mutex_t
      *
      * DESCRIPTION
      *      Destroys a mutex object and returns any resources
      *      to the system.
      *
      * RESULTS
      *              0               successfully initialized attr,
      *              EINVAL          not a valid mutex pointer,
      *              EBUSY           the mutex is currently locked.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  pthread_mutex_t mx;
  int oldCancelType;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  /*
   * We need to prevent us from being canceled
   * unexpectedly leaving the mutex in a corrupted state.
   * We can do this by temporarily
   * setting the thread to DEFERRED cancel type
   * and resetting to the original value whenever
   * we sleep and when we return. We also check if we've
   * been canceled at the same time.
   */
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldCancelType);

  /*
   * This waits until no other thread is looking at the
   * (possibly uninitialised) mutex object, then gives
   * us exclusive access.
   */
  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);

  if (mx == NULL)
    {
      result = EINVAL;
      goto FAIL0;
    }

  /*
   * We now have exclusive access to the mutex pointer
   * and structure, whether initialised or not.
   */

  /*
   * Check to see if we have something to delete.
   */
  if (mx != (pthread_mutex_t) PTW32_OBJECT_AUTO_INIT)
    {
      /*
       * Check to see if the mutex is held by any thread. We
       * can't destroy it if it is. Pthread_mutex_trylock is
       * not recursive and will return EBUSY even if the current
       * thread holds the lock.
       */
      if (mx->owner != NULL)
        {
          result = EBUSY;
        }
      else
        {
          free(mx);
          mx = NULL;
        }
    }
  else
    {
      /*
       * This is all we need to do to destroy an
       * uninitialised statically declared mutex.
       */
      mx = NULL;
    }

 FAIL0:
  PTW32_OBJECT_SET(mutex, mx);

  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
    {
      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_testcancel();
    }

  return result;
}

int
pthread_mutexattr_init (pthread_mutexattr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Initializes a mutex attributes object with default
      *      attributes.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_mutexattr_t
      *
      *
      * DESCRIPTION
      *      Initializes a mutex attributes object with default
      *      attributes.
      *
      *      NOTES:
      *              1)      Used to define mutex types
      *
      * RESULTS
      *              0               successfully initialized attr,
      *              ENOMEM          insufficient memory for attr.
      *
      * ------------------------------------------------------
      */
{
  pthread_mutexattr_t ma;
  int result = 0;

  ma = (pthread_mutexattr_t) calloc (1, sizeof (*ma));

  if (ma == NULL)
    {
      result = ENOMEM;
    }

  ma->pshared = PTHREAD_PROCESS_PRIVATE;
  ma->type = PTHREAD_MUTEX_DEFAULT;

  *attr = ma;

  return (result);

}                               /* pthread_mutexattr_init */


int
pthread_mutexattr_destroy (pthread_mutexattr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Destroys a mutex attributes object. The object can
      *      no longer be used.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_mutexattr_t
      *
      *
      * DESCRIPTION
      *      Destroys a mutex attributes object. The object can
      *      no longer be used.
      *
      *      NOTES:
      *              1)      Does not affect mutexes created using 'attr'
      *
      * RESULTS
      *              0               successfully released attr,
      *              EINVAL          'attr' is invalid.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;

  if (attr == NULL || *attr == NULL)
    {
      result = EINVAL;

    }
  else
    {
      pthread_mutexattr_t ma = *attr;

      *attr = NULL;
      free (ma);

      result = 0;
    }

  return (result);

}                               /* pthread_mutexattr_destroy */


int
pthread_mutexattr_getpshared (const pthread_mutexattr_t * attr,
			      int *pshared)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Determine whether mutexes created with 'attr' can be
      *      shared between processes.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_mutexattr_t
      *
      *      pshared
      *              will be set to one of:
      *
      *                      PTHREAD_PROCESS_SHARED
      *                              May be shared if in shared memory
      *
      *                      PTHREAD_PROCESS_PRIVATE
      *                              Cannot be shared.
      *
      *
      * DESCRIPTION
      *      Mutexes creatd with 'attr' can be shared between
      *      processes if pthread_mutex_t variable is allocated
      *      in memory shared by these processes.
      *      NOTES:
      *              1)      pshared mutexes MUST be allocated in shared
      *                      memory.
      *              2)      The following macro is defined if shared mutexes
      *                      are supported:
      *                              _POSIX_THREAD_PROCESS_SHARED
      *
      * RESULTS
      *              0               successfully retrieved attribute,
      *              EINVAL          'attr' is invalid,
      *
      * ------------------------------------------------------
      */
{
  int result;

  if ((attr != NULL && *attr != NULL) &&
      (pshared != NULL))
    {
      *pshared = (*attr)->pshared;
      result = 0;
    }
  else
    {
      *pshared = PTHREAD_PROCESS_PRIVATE;
      result = EINVAL;
    }

  return (result);

}                               /* pthread_mutexattr_getpshared */


int
pthread_mutexattr_setpshared (pthread_mutexattr_t * attr,
			      int pshared)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Mutexes created with 'attr' can be shared between
      *      processes if pthread_mutex_t variable is allocated
      *      in memory shared by these processes.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_mutexattr_t
      *
      *      pshared
      *              must be one of:
      *
      *                      PTHREAD_PROCESS_SHARED
      *                              May be shared if in shared memory
      *
      *                      PTHREAD_PROCESS_PRIVATE
      *                              Cannot be shared.
      *
      * DESCRIPTION
      *      Mutexes creatd with 'attr' can be shared between
      *      processes if pthread_mutex_t variable is allocated
      *      in memory shared by these processes.
      *
      *      NOTES:
      *              1)      pshared mutexes MUST be allocated in shared
      *                      memory.
      *
      *              2)      The following macro is defined if shared mutexes
      *                      are supported:
      *                              _POSIX_THREAD_PROCESS_SHARED
      *
      * RESULTS
      *              0               successfully set attribute,
      *              EINVAL          'attr' or pshared is invalid,
      *              ENOSYS          PTHREAD_PROCESS_SHARED not supported,
      *
      * ------------------------------------------------------
      */
{
  int result;

  if ((attr != NULL && *attr != NULL) &&
      ((pshared == PTHREAD_PROCESS_SHARED) ||
       (pshared == PTHREAD_PROCESS_PRIVATE)))
    {
      if (pshared == PTHREAD_PROCESS_SHARED)
        {

#if !defined( _POSIX_THREAD_PROCESS_SHARED )

          result = ENOSYS;
          pshared = PTHREAD_PROCESS_PRIVATE;

#else

          result = 0;

#endif /* _POSIX_THREAD_PROCESS_SHARED */

        }
      else
        {
          result = 0;
        }
      (*attr)->pshared = pshared;
    }
  else
    {
      result = EINVAL;
    }

  return (result);

}                               /* pthread_mutexattr_setpshared */


int
pthread_mutexattr_settype (pthread_mutexattr_t * attr,
                           int type)
     /*
      * ------------------------------------------------------
      *
      * DOCPUBLIC
      * The pthread_mutexattr_settype() and
      * pthread_mutexattr_gettype() functions  respectively set and
      * get the mutex type  attribute. This attribute is set in  the
      * type parameter to these functions.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_mutexattr_t
      *
      *      type
      *              must be one of:
      *
      *                      PTHREAD_MUTEX_DEFAULT
      *
      *                      PTHREAD_MUTEX_NORMAL
      *
      *                      PTHREAD_MUTEX_ERRORCHECK
      *
      *                      PTHREAD_MUTEX_RECURSIVE
      *
      * DESCRIPTION
      * The pthread_mutexattr_settype() and
      * pthread_mutexattr_gettype() functions  respectively set and
      * get the mutex type  attribute. This attribute is set in  the
      * type  parameter to these functions. The default value of the
      * type  attribute is  PTHREAD_MUTEX_DEFAULT.
      * 
      * The type of mutex is contained in the type  attribute of the
      * mutex attributes. Valid mutex types include:
      *
      * PTHREAD_MUTEX_NORMAL
      *          This type of mutex does  not  detect  deadlock.  A
      *          thread  attempting  to  relock  this mutex without
      *          first unlocking it will  deadlock.  Attempting  to
      *          unlock  a  mutex  locked  by  a  different  thread
      *          results  in  undefined  behavior.  Attempting   to
      *          unlock  an  unlocked  mutex  results  in undefined
      *          behavior.
      * 
      * PTHREAD_MUTEX_ERRORCHECK
      *          This type of  mutex  provides  error  checking.  A
      *          thread  attempting  to  relock  this mutex without
      *          first unlocking it will return with  an  error.  A
      *          thread  attempting to unlock a mutex which another
      *          thread has locked will return  with  an  error.  A
      *          thread attempting to unlock an unlocked mutex will
      *          return with an error.
      *
      * PTHREAD_MUTEX_DEFAULT
      *          Same as PTHREAD_MUTEX_RECURSIVE.
      * 
      * PTHREAD_MUTEX_RECURSIVE
      *          A thread attempting to relock this  mutex  without
      *          first  unlocking  it  will  succeed in locking the
      *          mutex. The relocking deadlock which can occur with
      *          mutexes of type  PTHREAD_MUTEX_NORMAL cannot occur
      *          with this type of mutex. Multiple  locks  of  this
      *          mutex  require  the  same  number  of  unlocks  to
      *          release  the  mutex  before  another  thread   can
      *          acquire the mutex. A thread attempting to unlock a
      *          mutex which another thread has locked will  return
      *          with  an  error. A thread attempting to  unlock an
      *          unlocked mutex will return  with  an  error.  This
      *          type  of mutex is only supported for mutexes whose
      *          process        shared         attribute         is
      *          PTHREAD_PROCESS_PRIVATE.
      *
      * RESULTS
      *              0               successfully set attribute,
      *              EINVAL          'attr' or 'type' is invalid,
      *
      * ------------------------------------------------------
      */
{
  int result = 0;

  if ((attr != NULL && *attr != NULL))
    {
      switch (type)
        {
        case PTHREAD_MUTEX_DEFAULT:
        case PTHREAD_MUTEX_NORMAL:
        case PTHREAD_MUTEX_ERRORCHECK:
        case PTHREAD_MUTEX_RECURSIVE:
          (*attr)->type = type;
          break;
        default:
          result = EINVAL;
          break;
        }
    }
  else
    {
      result = EINVAL;
    }

  return (result);

}                               /* pthread_mutexattr_settype */


int
pthread_mutexattr_gettype (pthread_mutexattr_t * attr,
                           int * type)
{
  int result = 0;

  if ((attr != NULL && *attr != NULL))
    {
      *type = (*attr)->type;
    }
  else
    {
      result = EINVAL;
    }

  return (result);
}


int
pthread_mutex_lock(pthread_mutex_t *mutex)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Locks an unlocked mutex. If the mutex is already
      *      locked, the calling thread usually blocks, but
      *      depending on the current owner and type of the mutex
      *      may recursively lock the mutex or return an error.
      *
      * PARAMETERS
      *      mutex
      *              pointer to an instance of pthread_mutex_t
      *
      * DESCRIPTION
      *      Locks an unlocked mutex. If the mutex is already
      *      locked, the calling thread usually blocks, but
      *      depending on the current owner and type of the mutex
      *      may recursively lock the mutex or return an error.
      *
      *      See the description under pthread_mutexattr_settype()
      *      for details.
      *
      * RESULTS
      *              0               successfully locked mutex,
      *              EINVAL          not a valid mutex pointer,
      *              ENOMEM          insufficient memory to initialise
      *                              the statically declared mutex object,
      *              EDEADLK         the mutex is of type
      *                              PTHREAD_MUTEX_ERRORCHECK and the
      *                              calling thread already owns the mutex.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  pthread_mutex_t mx;
  int oldCancelType;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  /*
   * We need to prevent us from being canceled
   * unexpectedly leaving the mutex in a corrupted state.
   * We can do this by temporarily
   * setting the thread to DEFERRED cancel type
   * and resetting to the original value whenever
   * we sleep and when we return. If we are set to
   * asynch cancelation then we also check if we've
   * been canceled at the same time.
   */
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldCancelType);

  /*
   * This waits until no other thread is looking at the
   * (possibly uninitialised) mutex object, then gives
   * us exclusive access.
   */
  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);

  if (mx == NULL)
    {
      result = EINVAL;
      goto FAIL0;
    }

  /*
   * We now have exclusive access to the mutex pointer
   * and structure, whether initialised or not.
   */

  if (mx == (pthread_mutex_t) PTW32_OBJECT_AUTO_INIT)
    {
      result = pthread_mutex_init(&mx, NULL);
    }

  if (result == 0)
    {
      pthread_t self = pthread_self();

      switch (mx->type)
        {
        case PTHREAD_MUTEX_DEFAULT:
        case PTHREAD_MUTEX_RECURSIVE:
          while (TRUE)
            {
              if (0 == ++mx->lock_idx)
                {
                  /*
                   * The lock is temporarily ours, but
                   * ensure that we give other waiting threads a
                   * chance to take the mutex if we held it last time.
                   */
                  if (mx->waiters > 0 && mx->lastOwner == self)
                    {
                      /*
                       * Check to see if other waiting threads
                       * have stopped waiting but haven't decremented
                       * the 'waiters' counter - ie. they may have been
                       * canceled. If we're wrong then waiting threads will
                       * increment the value again.
                       */
                      if (mx->lastWaiter == self)
                        {
                          mx->waiters = 0;
                        }
                      else
                        {
                          goto WAIT_RECURSIVE;
                        }
                    }
LOCK_RECURSIVE:
                  /*
                   * Take the lock.
                   */
                  mx->owner = self;
                  mx->lastOwner = self;
                  mx->lastWaiter = NULL;
                  break;
                }
              else
                {
                  while (mx->try_lock)
                    {
                      Sleep(0);
                    }

                  if (mx->owner == self)
                    {
                      goto LOCK_RECURSIVE;
                    }
WAIT_RECURSIVE:
                  mx->waiters++;
                  mx->lastWaiter = self;
                  mx->lock_idx--;
                  PTW32_OBJECT_SET(mutex, mx);
                  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
                    {
                      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
                      pthread_testcancel();
                      Sleep(0);
                      (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
                    }
                  else
                    {
                      Sleep(0);
                    }
                  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);
                  /*
                   * Thread priorities may have tricked another
                   * thread into thinking we weren't waiting anymore.
                   * If so, waiters will equal 0 so just don't
                   * decrement it.
                   */
                  if (mx->waiters > 0)
                    {
                      mx->waiters--;
                    }
                }
            }
          break;
        case PTHREAD_MUTEX_NORMAL:
          /*
           * If the thread already owns the mutex
           * then the thread will become deadlocked.
           */
          while (TRUE)
            {
              if (0 == ++mx->lock_idx)
                {
                  /*
                   * The lock is temporarily ours, but
                   * ensure that we give other waiting threads a
                   * chance to take the mutex if we held it last time.
                   */
                  if (mx->waiters > 0 && mx->lastOwner == self)
                    {
                      /*
                       * Check to see if other waiting threads
                       * have stopped waiting but haven't decremented
                       * the 'waiters' counter - ie. they may have been
                       * canceled. If we're wrong then waiting threads will
                       * increment the value again.
                       */
                      if (mx->lastWaiter == self)
                        {
                          mx->waiters = 0L;
                        }
                      else
                        {
                          goto WAIT_NORMAL;
                        }
                    }
                  /*
                   * Take the lock.
                   */
                  mx->owner = self;
                  mx->lastOwner = self;
                  mx->lastWaiter = NULL;
                  break;
                }
              else
                {
                  while (mx->try_lock)
                    {
                      Sleep(0);
                    }
WAIT_NORMAL:
                  mx->waiters++;
                  mx->lastWaiter = self;
                  mx->lock_idx--;
                  PTW32_OBJECT_SET(mutex, mx);
                  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
                    {
                      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
                      pthread_testcancel();
                      Sleep(0);
                      (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
                    }
                  else
                    {
                      Sleep(0);
                    }
                  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);
                  /*
                   * Thread priorities may have tricked another
                   * thread into thinking we weren't waiting anymore.
                   * If so, waiters will equal 0 so just don't
                   * decrement it.
                   */
                  if (mx->waiters > 0)
                    {
                      mx->waiters--;
                    }
                }
            }
          break;
        case PTHREAD_MUTEX_ERRORCHECK:
          while (TRUE)
            {
              if (0 == ++mx->lock_idx)
                {
                  /*
                   * The lock is temporarily ours, but
                   * ensure that we give other waiting threads a
                   * chance to take the mutex if we held it last time.
                   */
                  if (mx->waiters > 0 && mx->lastOwner == self)
                    {
                      /*
                       * Check to see if other waiting threads
                       * have stopped waiting but haven't decremented
                       * the 'waiters' counter - ie. they may have been
                       * canceled. If we're wrong then waiting threads will
                       * increment the value again.
                       */
                      if (mx->lastWaiter == self)
                        {
                          mx->waiters = 0L;
                        }
                      else
                        {
                          goto WAIT_ERRORCHECK;
                        }
                    }
                  /*
                   * Take the lock.
                   */
                  mx->owner = self;
                  mx->lastOwner = self;
                  mx->lastWaiter = NULL;
                  break;
                }
              else
                {
                  while (mx->try_lock)
                    {
                      Sleep(0);
                    }

                  if (mx->owner == self)
                    {
                      result = EDEADLK;
                      break;
                    }
WAIT_ERRORCHECK:
                  mx->waiters++;
                  mx->lastWaiter = self;
                  mx->lock_idx--;
                  PTW32_OBJECT_SET(mutex, mx);
                  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
                    {
                      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
                      pthread_testcancel();
                      Sleep(0);
                      (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
                    }
                  else
                    {
                      Sleep(0);
                    }
                  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);
                  /*
                   * Thread priorities may have tricked another
                   * thread into thinking we weren't waiting anymore.
                   * If so, waiters will equal 0 so just don't
                   * decrement it.
                   */
                  if (mx->waiters > 0)
                    {
                      mx->waiters--;
                    }
                }
            }
          break;
        default:
          result = EINVAL;
          break;
        }
    }

 FAIL0:
  PTW32_OBJECT_SET(mutex, mx);

  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
    {
      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_testcancel();
    }

  return result;
}

int
pthread_mutex_unlock(pthread_mutex_t *mutex)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Decrements the lock count of the currently locked mutex.
      *
      * PARAMETERS
      *      mutex
      *              pointer to an instance of pthread_mutex_t
      *
      * DESCRIPTION
      *      Decrements the lock count of the currently locked mutex.
      *
      *      If the count reaches it's 'unlocked' value then it
      *      is available to be locked by another waiting thread.
      *      The implementation ensures that other waiting threads
      *      get a chance to take the unlocked mutex before the unlocking
      *      thread can re-lock it.
      *
      * RESULTS
      *              0               successfully locked mutex,
      *              EINVAL          not a valid mutex pointer,
      *              EPERM           the current thread does not own
      *                              the mutex.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  pthread_mutex_t mx;
  int oldCancelType;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  /*
   * We need to prevent us from being canceled
   * unexpectedly leaving the mutex in a corrupted state.
   * We can do this by temporarily
   * setting the thread to DEFERRED cancel type
   * and resetting to the original value whenever
   * we sleep and when we return. If we are set to
   * asynch cancelation then we also check if we've
   * been canceled at the same time.
   */
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldCancelType);

  /*
   * This waits until no other thread is looking at the
   * (possibly uninitialised) mutex object, then gives
   * us exclusive access.
   */
  PTW32_OBJECT_GET(pthread_mutex_t, mutex, mx);

  if (mx == NULL)
    {
      result = EINVAL;
      goto FAIL0;
    }

  /*
   * We now have exclusive access to the mutex pointer
   * and structure, whether initialised or not.
   */

  if (mx != (pthread_mutex_t) PTW32_OBJECT_AUTO_INIT
      && mx->owner == pthread_self())
    {
      switch (mx->type)
        {
        case PTHREAD_MUTEX_NORMAL:
        case PTHREAD_MUTEX_ERRORCHECK:
          mx->owner = NULL;
          break;
        case PTHREAD_MUTEX_RECURSIVE:
        default:
          if (mx->lock_idx == 0)
            {
              mx->owner = NULL;
            }
          break;
        }

      mx->lock_idx--;
    }
  else
    {
      result = EPERM;
    }

 FAIL0:
  PTW32_OBJECT_SET(mutex, mx);

  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
    {
      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_testcancel();
    }

  return result;
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Tries to lock a mutex. If the mutex is already
      *      locked (by any thread, including the calling thread),
      *      the calling thread returns without waiting
      *      for the mutex to be freed (nor recursively locking
      *      the mutex if the calling thread currently owns it).
      *
      * PARAMETERS
      *      mutex
      *              pointer to an instance of pthread_mutex_t
      *
      * DESCRIPTION
      *      Tries to lock a mutex. If the mutex is already
      *      locked (by any thread, including the calling thread),
      *      the calling thread returns without waiting
      *      for the mutex to be freed (nor recursively locking
      *      the mutex if the calling thread currently owns it).
      *
      * RESULTS
      *              0               successfully locked the mutex,
      *              EINVAL          not a valid mutex pointer,
      *              EBUSY           the mutex is currently locked,
      *              ENOMEM          insufficient memory to initialise
      *                              the statically declared mutex object.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  pthread_mutex_t mx;
  pthread_t self;
  int oldCancelType;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  /*
   * We need to prevent us from being canceled
   * unexpectedly leaving the mutex in a corrupted state.
   * We can do this by temporarily
   * setting the thread to DEFERRED cancel type
   * and resetting to the original value whenever
   * we sleep and when we return. If we are set to
   * asynch cancelation then we also check if we've
   * been canceled at the same time.
   */
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldCancelType);

  /*
   * If no other thread is looking at the
   * (possibly uninitialised) mutex object, then gives
   * us exclusive access, otherwise returns immediately.
   */
  if (!PTW32_OBJECT_TRYGET(pthread_mutex_t, mutex, mx))
    {
      result = EBUSY;
      goto FAIL0;
    }

  if (mx == NULL)
    {
      result = EINVAL;
      goto FAIL0;
    }

  /*
   * We now have exclusive access to the mutex pointer
   * and structure, whether initialised or not.
   */

  if (mx == (pthread_mutex_t) PTW32_OBJECT_AUTO_INIT)
    {
      result = pthread_mutex_init(&mx, NULL);
    }

  if (result == 0)
    {
      self = pthread_self();
      /*
       * Trylock returns EBUSY if the mutex is held already,
       * even if the current thread owns the mutex - ie. it
       * doesn't lock it recursively, even
       * if the mutex type is PTHREAD_MUTEX_RECURSIVE.
       */
      if (0 == (mx->lock_idx + 1))
        {
          mx->try_lock++;

          if (0 == InterlockedIncrement(&mx->lock_idx))
            {
              mx->owner = self;
              mx->lastOwner = self;
              mx->lastWaiter = NULL;
            }
          else
            {
              InterlockedDecrement(&mx->lock_idx);
              result = EBUSY;
            }

          mx->try_lock--;
        }
      else
        {
          result = EBUSY;
        }
    }

  PTW32_OBJECT_SET(mutex, mx);

 FAIL0:
  if (oldCancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
    {
      (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_testcancel();
    }

  return result;
}

