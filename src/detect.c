/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2001, 2002  Tim Waugh <twaugh@redhat.com>
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

#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#if !(defined __MINGW32__ || defined _MSC_VER)
#include <sys/ioctl.h>
#else
#include <io.h> /* open(), close() */
#define O_NOCTTY 0
#define S_ISDIR(mode) ((mode) & _S_IFDIR ? 1 : 0)
#endif
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __unix__
#include <unistd.h>
#endif

#include "debug.h"
#include "detect.h"

#ifdef HAVE_LINUX
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif /* HAVE_SYS_IO_H */
#include "ppdev.h"
#elif defined(HAVE_OBSD_I386)
/* for i386_get_ioperm and i386_set_ioperm */
#include <machine/sysarch.h>
#elif defined(HAVE_SOLARIS)
#include <sys/ddi.h>
#include <sys/sunddi.h>
#elif defined(HAVE_CYGWIN_NT)
#ifdef __CYGWIN__
#include <w32api/windows.h>
#else
#include <windows.h>
#endif
#endif


int capabilities;

/* Look for parport entries in /proc.
 * Linux 2.2.x has /proc/parport/.
 * Linux 2.4.x has /proc/sys/dev/parport/. */
static int
check_proc_type (void)
{
  int which = 0;
  struct stat st;
  if (stat ("/proc/sys/dev/parport", &st) == 0 &&
      S_ISDIR (st.st_mode))
    {
      which = PROC_SYS_DEV_PARPORT_CAPABLE;
      debugprintf ("This system has /proc/sys/dev/parport\n");
    }
  else if (stat ("/proc/parport", &st) == 0 &&
	   S_ISDIR (st.st_mode) &&
	   st.st_nlink > 2)
    {
      which = PROC_PARPORT_CAPABLE;
      debugprintf ("This system has /proc/parport\n");
    }
  capabilities |= which;
  return which;
}

/* Try to find a device node that works. */
static int
check_dev_node (const char *type)
{
  char name[50]; /* only callers are in detect_environment */
  int fd;
  int i;
#ifdef HAVE_LINUX
  int is_parport;
#endif

  for (i = 0; i < 8; i++) {
    sprintf (name, "/dev/%s%d", type, i);
    fd = open (name, O_RDONLY | O_NOCTTY);

#ifdef HAVE_LINUX
    is_parport = !strncmp (type, "parport", 7);

    if ((fd < 0) && is_parport) {
      /* Try with udev/devfs naming */
      debugprintf("%s isn't accessible, retrying with udev/devfs naming...\n", name);
      sprintf (name, "/dev/%ss/%d", type, i);
      fd = open (name, O_RDONLY | O_NOCTTY);
    }
#endif

    if (fd >= 0) {
#ifdef HAVE_LINUX
      if (is_parport)
	{
	  /* Try to claim the device.  This will
	   * force the low-level port driver to get loaded. */
	  if (ioctl (fd, PPCLAIM) == 0)
	    ioctl (fd, PPRELEASE);
	}
#endif

      close (fd);
      debugprintf ("%s is accessible\n", name);
      return 1;
    } else {
      debugprintf("%s isn't accessible\n", name);
    }
  }

  return 0;
}

/* Is /dev/port accessible? */
static int
check_dev_port (void)
{
  int fd = open ("/dev/port", O_RDWR | O_NOCTTY);
  if (fd >= 0) {
    close (fd);
    capabilities |= DEV_PORT_CAPABLE;
    debugprintf ("/dev/port is accessible\n");
    return 1;
  }
  return 0;
}

/* Can we use direct I/O with inb and outb? */
static int
check_io (void)
{
  #ifdef HAVE_OBSD_I386
  u_long *iomap;
  if ((iomap = malloc(1024/8)) == NULL) return 0;
  if ((i386_get_ioperm(iomap) == 0) && (i386_set_ioperm(iomap) == 0)) {
    capabilities |= IO_CAPABLE;
    debugprintf ("We can use i386_get_ioperm()\n");
    free(iomap);
    return 1;
  }
  free(iomap);
  #elif defined(HAVE_FBSD_I386)
  int fd;
  if ((fd = open("/dev/io", O_RDONLY)) >= 0) {
    capabilities |= IO_CAPABLE;
    debugprintf("We can use /dev/io\n");
    close(fd);
    return 1;
  }
  #elif defined(HAVE_LINUX)
  #ifdef HAVE_SYS_IO_H
  if (ioperm (0x378 /* say */, 3, 1) == 0) {
    ioperm (0x378, 3, 0);
    capabilities |= IO_CAPABLE;
    debugprintf ("We can use ioperm()\n");
    return 1;
  }
  #else
  debugprintf ("We cannot use ioperm() : not supported\n");
  return 0;
  #endif /* HAVE_SYS_IO_H */
  
  #elif defined(HAVE_SOLARIS)
  int fd;
  if (fd=open("/devices/pseudo/iop@0:iop", O_RDWR) > 0) {
    capabilities |= IO_CAPABLE;
    debugprintf ("We can use iop\n");
    return 1;
  }
  debugprintf ("We can't use IOP, nothing will work\n");
  #elif defined(HAVE_CYGWIN_9X)
  /* note: 95 allows apps direct IO access */
  debugprintf ("Taking a guess on port availability (win9x)\n");
  capabilities |= IO_CAPABLE;
  return 1;
  #endif

  return 0;
}

/* Can we use win32 style I/O (cygwin environment) */

static int
check_lpt (void)
{
  #ifdef HAVE_CYGWIN_NT

  HANDLE hf = CreateFile("\\\\.\\$VDMLPT1", GENERIC_READ | GENERIC_WRITE,
      0, NULL, OPEN_EXISTING, 0, NULL);
  if (hf == INVALID_HANDLE_VALUE) return 0;
  CloseHandle(hf);

  capabilities |= LPT_CAPABLE;
  return 1;
  #else
  return 0;
  #endif
}

/* Figure out what we can use to talk to the parallel port.
 * We aren't allowed to use the things set in forbidden though. */
int
detect_environment (int forbidden)
{
#define FORBIDDEN(bit) (forbidden & bit)
  int dev_node_parport = 0;
  static int detected = 0;
  if (detected && !forbidden) return 0;
  detected = 1;

  capabilities = 0;

  /* Find out what access mechanisms there are. */
  if (!FORBIDDEN(PPDEV_CAPABLE))
    dev_node_parport = check_dev_node ("parport");
  if (dev_node_parport)
    capabilities |= PPDEV_CAPABLE;
  if (!FORBIDDEN (IO_CAPABLE))
    check_io ();
  if (!FORBIDDEN (DEV_PORT_CAPABLE))
    check_dev_port ();
  if (!FORBIDDEN (LPT_CAPABLE))
    check_lpt ();

  /* Find out what kind of /proc structure we have. */
  if (!dev_node_parport) /* Don't load lp if we'll use ppdev (claim will fail if F1284_EXCL). */
    check_dev_node ("lp"); /* causes low-level port driver to be loaded */
  check_proc_type ();

  return 0;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
