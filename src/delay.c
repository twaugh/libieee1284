/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2001  Tim Waugh <twaugh@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/time.h>
#include <unistd.h>

#include "delay.h"

void udelay(unsigned long usec)
{
	struct timeval now, deadline;
	
	gettimeofday(&deadline, NULL);
	deadline.tv_usec += usec;
	deadline.tv_sec += deadline.tv_usec / 1000000;
	deadline.tv_usec %= 1000000;
	
	do {
		gettimeofday(&now, NULL);
	} while ((now.tv_sec < deadline.tv_sec) || 
		(now.tv_sec == deadline.tv_sec &&
		now.tv_usec < deadline.tv_usec));
}

