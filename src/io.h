/* Redefine inb and outb_p for 95 and OBSD because they don't have sys/io.h */

#ifndef _IO_H
  
#define _IO_H

#ifndef _MSC_VER

static __inline unsigned char
inb (unsigned short int port)
{
  unsigned char _v;
  __asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
  return _v;
}

static __inline void
outb_p (unsigned char value, unsigned short int port)
{
  __asm__ __volatile__ ("outb %b0,%w1\noutb %%al,$0x80": :"a" (value),
			"Nd" (port));
}

#else

#include <conio.h>

static __inline unsigned char
inb (unsigned short int port)
{
  return inp (port);
}

static __inline void
outb_p (unsigned char value, unsigned short int port)
{
  outp (port, value);
}

#endif /* _MSC_VER */

#endif /* _IO_H */
