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


struct parport_internal
{
  int type;
  char *device;
  unsigned long base;
  unsigned long base_hi;
  int fd;
  int claimed;
  int flags;
};

#define IO_CAPABLE			(1<<0)
#define PPDEV_CAPABLE			(1<<1)
#define PROC_PARPORT_CAPABLE		(1<<2)
#define PROC_SYS_DEV_PARPORT_CAPABLE	(1<<3)
#define DEV_LP_CAPABLE			(1<<4)
#define DEV_PORT_CAPABLE		(1<<5)
extern int capabilities;

extern int detect_environment (int forbidden);

struct raw_routines_struct
{
  unsigned char (*inb) (unsigned long);
  void (*outb) (unsigned char, unsigned long);
};
extern struct raw_routines_struct raw_routines;
#define INB(x) raw_routines.inb(x)
#define OUTB(x,y) raw_routines.outb(x,y)

extern void use_dev_port (int on);
