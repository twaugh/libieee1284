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

#include <fcntl.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "detect.h"

int capabilities;

static unsigned char raw_inb (unsigned long port)
{
  return inb (port);
}

static void raw_outb (unsigned char val, unsigned long port)
{
  outb_p (val, port);
}

static int dev_port_fd = -1;
static unsigned char port_inb (unsigned long port)
{
  unsigned char byte = 0xff;
  if (dev_port_fd == -1)
    {
      dev_port_fd = open ("/dev/port", O_RDWR | O_NOCTTY);
      if (dev_port_fd < 0)
	return byte;
    }

  if (lseek (dev_port_fd, port, SEEK_SET) != (off_t)-1)
    read (dev_port_fd, &byte, 1);

  return byte;
}

static void port_outb (unsigned char val, unsigned long port)
{
  if (dev_port_fd == -1)
    {
      dev_port_fd = open ("/dev/port", O_RDWR | O_NOCTTY);
      if (dev_port_fd < 0)
	return;
    }

  if (lseek (dev_port_fd, port, SEEK_SET) != (off_t)-1)
    write (dev_port_fd, &val, 1);
}

struct raw_routines_struct raw_routines = { raw_inb, raw_outb };

void use_dev_port (int on)
{
  if (on)
    {
      raw_routines.inb = port_inb;
      raw_routines.outb = port_outb;
    }
  else
    {
      raw_routines.inb = raw_inb;
      raw_routines.outb = raw_outb;
    }
}

// Look for parport entries in /proc.
// Linux 2.2.x has /proc/parport/.
// Linux 2.4.x has /proc/sys/dev/parport/.
static int check_proc_type (void)
{
	int which = 0;
	struct stat st;
	if (stat ("/proc/sys/dev/parport", &st) == 0 &&
	    st.st_mode & S_IFDIR &&
	    st.st_nlink > 2)
		which = PROC_SYS_DEV_PARPORT_CAPABLE;
	else if (stat ("/proc/parport", &st) == 0 &&
		 st.st_mode & S_IFDIR &&
		 st.st_nlink > 2)
		which = PROC_PARPORT_CAPABLE;
	capabilities |= which;
	return which;
}

// Try to find a device node that works.
static int check_dev_node (const char *type)
{
	char name[50]; // only callers are in detect_environment
	int fd;
	int i;

	for (i = 0; i < 8; i++) {
		sprintf (name, "/dev/%s%d", type, i);
		fd = open (name, O_RDONLY | O_NOCTTY);
		if (fd >= 0) {
			close (fd);
			return 1;
		}
	}

	return 0;
}

// Is /dev/port accessible?
static int check_dev_port (void)
{
	int fd = open ("/dev/port", O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		close (fd);
		capabilities |= DEV_PORT_CAPABLE;
		return 1;
	}
	return 0;
}

// Can we use direct I/O with inb and outb?
static int check_io (void)
{
	if (ioperm (0x378 /* say */, 3, 1) == 0) {
		ioperm (0x378, 3, 0);
		capabilities |= IO_CAPABLE;
		return 1;
	}
	return 0;
}

// Figure out what we can use to talk to the parallel port.
// We aren't allowed to use the things set in forbidden though.
int detect_environment (int forbidden)
{
#define FORBIDDEN(bit) (forbidden & bit)
	static int detected = 0;
	if (detected && !forbidden) return 0;
	detected = 1;

	capabilities = 0;

	// Find out what access mechanisms there are.
	if ((!FORBIDDEN(PPDEV_CAPABLE)) && check_dev_node ("parport"))
		capabilities |= PPDEV_CAPABLE;
	if (!FORBIDDEN (IO_CAPABLE))
		check_io ();
	if (!FORBIDDEN (DEV_PORT_CAPABLE))
		check_dev_port ();

	// Find out what kind of /proc structure we have.
	if (!check_proc_type ()) {
		// Maybe the low-level port driver wasn't loaded.
		if ((!FORBIDDEN (DEV_LP_CAPABLE)) && check_dev_node ("lp"))
			capabilities |= DEV_LP_CAPABLE;

		check_proc_type ();
	}

	return 0;
}
