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

struct config_variables conf;

static const char *const ieee1284conf = "ieee1284.conf";
static const size_t max_line_len = 1000;
static const char *ifs = " \t\n";

/* Get the next token.  Caller frees returned zero-terminated string. */
static char *get_token (FILE *f)
{
  static char *current_line = NULL;
  static size_t current_line_len = 0;
  static size_t at = 0;
  size_t end, i;
  char *this_token;

  for (;;)
    {
      int quotes = 0;

      if (at == current_line_len)
	{
	  if (current_line)
	    free (current_line);

	  current_line = NULL;
	  current_line_len = 0;
	  at = 0;
	}

      if (!current_line)
	{
	  current_line = malloc (sizeof (char) * max_line_len);
	  if (!current_line)
	    return NULL;

	  /* Ideally we'd use getline here, but that isn't available
	   * everywhere. In fact, *ideally* we'd use wordexp for this
	   * whole function, but that isn't widely available either. */
	  if (!fgets (current_line, max_line_len, f))
	    {
	      free (current_line);
	      current_line = NULL;
	      current_line_len = 0;
	      at = 0;
	      return NULL;
	    }

	  current_line_len = strlen (current_line);
	  at = 0;
	}

      /* Skip whitespace. */
      at += strspn (current_line + at, ifs);

      if (current_line[at] == '#')
	{
	  /* Ignore the rest of the line. */
	  at = current_line_len;
	  continue;
	}

      /* Find the end of the token. */
      for (end = at; end < current_line_len; end++)
	{
	  char ch = current_line[end];

	  if (ch == '\\' && quotes != 1)
	    {
	      end++;
	      continue;
	    }

	  if (ch == '\'')
	    {
	      if (quotes == 0)
		{
		  quotes = 1;
		  continue;
		}

	      if (quotes == 1)
		quotes = 0;
	    }

	  if (ch == '"')
	    {
	      if (quotes == 0)
		{
		  quotes = 2;
		  continue;
		}

	      if (quotes == 2)
		quotes = 0;
	    }

	  if (!quotes && strchr (ifs, ch))
	    break;
	}

      if (at == end)
	/* Next line. */
	continue;

      /* Copy this token. */
      this_token = malloc (sizeof (char) * (end - at + 1)); /* worst case */
      if (!this_token)
	return NULL;

      quotes = 0;
      for (i = 0; at < end; at++)
	{
	  char ch = current_line[at];

	  if (ch == '\\' && quotes != 1)
	    {
	      if (at < end - 1)
		this_token[i++] = current_line[++at];

	      continue;
	    }

	  if (ch == '\'')
	    {
	      if (quotes == 0)
		{
		  quotes = 1;
		  continue;
		}

	      if (quotes == 1)
		{
		  quotes = 0;
		  continue;
		}
	    }

	  if (ch == '"')
	    {
	      if (quotes == 0)
		{
		  quotes = 2;
		  continue;
		}

	      if (quotes == 2)
		{
		  quotes = 0;
		  continue;
		}
	    }

	  this_token[i++] = ch;
	}

      this_token[i] = '\0';
      break;
    }

  return this_token;
}

static void disallow (FILE *f)
{
  char *token = NULL;
  int i;

  for (i = 0; i < 2; i++)
    {
      if (token)
	free (token);

      token = get_token (f);
      if (!token)
	break;

      if (!strcmp (token, "ppdev"))
	{
	  dprintf ("* Disallowing ppdev\n");
	  conf.disallow_ppdev = 1;
	  break;
	}
      else
	{
	  dprintf ("Skipping unknown token: %s\n", token);
	  if (i)
	    dprintf ("Ignoring: disallow\n");
	}
    }

  if (token)
    free (token);
}

static int try_read_config_file (const char *path)
{
  FILE *f = fopen (path, "r");
  char *token;

  if (!f)
    return 1;

  dprintf ("Reading configuration from %s:\n", path);

  do
    {
      token = get_token (f);
      if (token)
	{
	  if (!strcmp (token, "disallow"))
	    disallow (f);
	  else
	    dprintf ("Skipping unknown word: %s\n", token);

	  free (token);
	}
    }
  while (token);

  fclose (f);
  dprintf ("End of configuration\n");
  return 0;
}

void
read_config_file (void)
{
  static int config_read = 0;
  char *path;

  if (config_read)
    return;

  conf.disallow_ppdev = 0;

  path = malloc (1 + 5 + rclen);
  if (!path)
    return;

  memcpy (path, "/etc/", 5);
  memcpy (path + 5, ieee1284conf, strlen (ieee1284conf) + 1);
  if (try_read_config_file (path))
    config_read = 1;

  free (path);
  return;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
