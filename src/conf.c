/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2002  Tim Waugh <twaugh@redhat.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>

#include "conf.h"
#include "debug.h"

static const char *const ieee1284rc = "ieee1284rc";

static int try_read_config_file (const char *path)
{
  FILE *f = fopen (path, "r");

  if (!f)
    return 1;

  dprintf ("Reading configuration from %s:\n", path);
  fclose (f);
  dprintf ("End of configuration\n");
  return 0;
}

void
read_config_file (void)
{
  static int config_read = 0;
  size_t rclen;
  const char *home;
  char *path;

  if (config_read)
    return;

  rclen = strlen (ieee1284rc);
  home = getenv ("HOME");
  if (home)
    {
      size_t homelen = strlen (home);
      path = malloc (1 + homelen + 2 + rclen);
      if (!path)
	  return;

      memcpy (path, home, homelen);
      memcpy (path + homelen, "/.", 2);
      memcpy (path + homelen + 2, ieee1284rc, rclen + 1);
      if (!try_read_config_file (path))
	{
	  /* Success. */
	  config_read = 1;
	  return;
	}
    }

  path = malloc (1 + 5 + rclen);
  if (!path)
    return;

  memcpy (path, "/etc/", 5);
  memcpy (path + 5, ieee1284rc, rclen + 1);
  if (try_read_config_file (path))
    config_read = 1;

  return;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
