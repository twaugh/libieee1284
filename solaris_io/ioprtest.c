#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define IOPREAD 1
#define IOPWRITE 2

typedef struct iopbuf_struct {
        unsigned int port;
        unsigned char port_value;
} iopbuf;

int
main()
{
    iopbuf tmpbuf;
    int fd=0;
    
    if((fd=open("/devices/pseudo/iop@0:iop", O_RDONLY)) < 0)
    {
        perror("OPEN failed\n");
    }

    tmpbuf.port_value = 0;

    tmpbuf.port=0x80; 
    if(ioctl(fd, IOPREAD, &tmpbuf))
      perror("IOCTL failed\n");
    printf("Port %x : %x\n", tmpbuf.port, tmpbuf.port_value);

    return 0;
}





