/* NT Parport Access stuff - Matthew Duggan (2002) */

/* 
 * ParallelVdm Device (0x2C) is mostly undocumented, used by VDM for parallel 
 * port compatibility.
 */

/* 
 * Different from CTL_CODE in DDK, limited to ParallelVdm but makes this 
 * code cleaner.
 */

#ifndef _PAR_NT_H

#define _PAR_NT_H

#define NT_CTL_CODE( Function ) ( (0x2C<<16) | ((Function) << 2) )

/* IOCTL codes */
#define NT_IOCTL_DATA      NT_CTL_CODE(1) /* Write Only */
#define NT_IOCTL_CONTROL   NT_CTL_CODE(2) /* Read/Write */
#define NT_IOCTL_STATUS    NT_CTL_CODE(3) /* Read Only  */

#endif
