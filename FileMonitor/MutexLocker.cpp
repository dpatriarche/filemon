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

#include <stdio.h>
#include <stdlib.h>

#include "MutexLocker.h"

//-----------------------------------------------------------------------------

MutexLocker_t::MutexLocker_t(pthread_mutex_t * mutex_p)
    : mutex_pm(mutex_p)
{
    if (pthread_mutex_lock(mutex_pm) != 0) {
        perror(NULL);
        exit(-1);
    }
}

//-----------------------------------------------------------------------------

MutexLocker_t::~MutexLocker_t()
{
    if (pthread_mutex_unlock(mutex_pm) != 0) {
        perror(NULL);
        exit(-1);
    }
}
