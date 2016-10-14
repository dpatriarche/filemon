#ifndef __INC_MutexLocker_H
#define __INC_MutexLocker_H

/*
 * Copyright 2008-2016 Douglas Patriarche
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pthread.h"

// This class performs the simple function of locking a mutex in its constructor and unlocking the mutex in its destructor. This is useful in that if an instance of this class is created in some scope, then when the scope ends the object will be automatically destructed, unlocking the mutex without the programmer having to remember to write any code.
class MutexLocker_t
{
private:
    
    pthread_mutex_t * mutex_pm;

public:

    // Constructor.
    MutexLocker_t(pthread_mutex_t * mutex_p);

    // Destructor.
    ~MutexLocker_t();
};

// This macro provides a slightly simpler and more obvious way of creating a MutexLocker_t instance.
#define MUTEX_LOCK_UNTIL_SCOPE_EXIT(mutex_p) \
    MutexLocker_t __mutexLocker(mutex_p);

#endif // __INC_MutexLocker_H
