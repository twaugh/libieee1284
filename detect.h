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

// The virtualized interface.  This allows different implementations
// of each function, without the runtime hit of having to decide which
// implementation to use every time the function is called.

#ifndef _DETECT_H_
#define _DETECT_H_

#include <stdlib.h>
#include <sys/time.h>

struct parport_internal;

struct parport_access_methods
{
  int (*init) (struct parport_internal *port);
  void (*cleanup) (struct parport_internal *port);

  unsigned char (*inb) (struct parport_internal *port, unsigned long addr);
  void (*outb) (struct parport_internal *port, unsigned char val,
		unsigned long addr);

  int (*read_data) (struct parport_internal *port);
  void (*write_data) (struct parport_internal *port, unsigned char st);
  void (*data_dir) (struct parport_internal *port, int reverse);

  int (*read_status) (struct parport_internal *port);
  int (*wait_status) (struct parport_internal *port,
		      unsigned char mask, unsigned char val,
		      struct timeval *timeout);

  int (*read_control) (struct parport_internal *port);
  void (*write_control) (struct parport_internal *port,
			 unsigned char ct);
  void (*frob_control) (struct parport_internal *port,
			unsigned char mask, unsigned char val);

  int (*do_nack_handshake) (struct parport_internal *port,
			    unsigned char ct_before,
			    unsigned char ct_after,
			    struct timeval *timeout);

  int (*negotiate) (struct parport_internal *port, int mode);
  void (*terminate) (struct parport_internal *port);

  int (*ecp_fwd_to_rev) (struct parport_internal *port);
  int (*ecp_rev_to_fwd) (struct parport_internal *port);

  ssize_t (*nibble_read) (struct parport_internal *port,
			  char *buffer, size_t len);
  ssize_t (*compat_write) (struct parport_internal *port,
			   const char *buffer, size_t len);
  ssize_t (*byte_read) (struct parport_internal *port,
			char *buffer, size_t len);
  ssize_t (*epp_read_data) (struct parport_internal *port, int flags,
			    char *buffer, size_t len);
  ssize_t (*epp_write_data) (struct parport_internal *port, int flags,
			     const char *buffer, size_t len);
  ssize_t (*epp_read_addr) (struct parport_internal *port, int flags,
			    char *buffer, size_t len);
  ssize_t (*epp_write_addr) (struct parport_internal *port, int flags,
			     const char *buffer, size_t len);
  ssize_t (*ecp_read_data) (struct parport_internal *port, int flags,
			    char *buffer, size_t len);
  ssize_t (*ecp_write_data) (struct parport_internal *port, int flags,
			     const char *buffer, size_t len);
  ssize_t (*ecp_read_addr) (struct parport_internal *port, int flags,
			    char *buffer, size_t len);
  ssize_t (*ecp_write_addr) (struct parport_internal *port, int flags,
			     const char *buffer, size_t len);
};

struct parport_internal
{
  int type;
  char *device;
  unsigned long base;
  unsigned long base_hi;
  int interrupt;
  int fd;
  int claimed;
  int flags;
  unsigned char ctr;
  int *selectable_fd;

  /* IEEE 1284 stuff */
  int current_mode;
  int current_channel;

  const struct parport_access_methods *fn;
};

#define IO_CAPABLE			(1<<0)
#define PPDEV_CAPABLE			(1<<1)
#define PROC_PARPORT_CAPABLE		(1<<2)
#define PROC_SYS_DEV_PARPORT_CAPABLE	(1<<3)
#define DEV_LP_CAPABLE			(1<<4)
#define DEV_PORT_CAPABLE		(1<<5)
extern int capabilities;

extern int detect_environment (int forbidden);

#endif /* _DETECT_H_ */

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
