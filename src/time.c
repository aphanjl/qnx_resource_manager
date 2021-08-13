#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

struct Timeattr_s;
#define IOFUNC_ATTR_T struct Timeattr_s
struct Timeocb_s;
#define IOFUNC_OCB_T struct Timeocb_s;

#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <string.h>

// 定义自己设备属性结构
typedef struct Timeattr_s
{
    iofunc_attr_t attr;
    char* format;
} Timeattr_t;

// 定义自己open context block 结构
typedef struct Timeocb_s
{
    iofunc_ocb_t ocb;
    char* buffer;
    int   bufsize;
} Timeocb_t;

#define NUMDEVICES 3
#define MAX_FORMAT_SIZE 64

#define TIME_DIR "dev/time"
char *devnames[NUMDEVICES] = 
{
    TIME_DIR "/now",
    TIME_DIR "/hour",
    TIME_DIR "/min"
};

char formats[NUMDEVICES][MAX_FORMAT_SIZE + 1] =
{
    "%Y %m %d %H:%M:%S\n",
    "%H\n",
    "%M\n"
};

int pathnameID [NUMDEVICES];
Timeattr_t timeattrs [NUMDEVICES];

void options(int argc, char**argv);
int io_read(resmgr_context_t *ctp, io_read_t *msg, Timeocb_t *tocb);
int io_write(resmgr_context_t *ctp, io_write_t *msg, Timeocb_t *tocb);
Timeocb_t *time_ocb_calloc(resmgr_context_t *ctp, Timeattr_t *tattr);
void time_ocb_free(Timeocb_t *tocb);
char *format_time(char *format, int time_offset);

resmgr_connect_funcs_t connect_funcs;
resmgr_io_funcs_t io_funcs;

iofunc_funcs_t time_ocb_funcs = 
{
    _IOFUNC_NFUNCS,
    time_ocb_calloc,
    time_ocb_free
};

iofunc_mount_t time_mount = 
{
    0, 0, 0, 0, &time_ocb_funcs
};

dispatch_t *dpp;
resmgr_attr_t rattr;
dispatch_context_t *ctp; 

char *programe = "time";
int optv;

int main(int argc, char **argv)
{
    int i ;
    options(argc, argv);

    dpp = dispatch_create();
    memset(&rattr, 0, sizeof(rattr));

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read  = io_read;
    io_funcs.write = io_write;

    for (i = 0; i < NUMDEVICES; ++i)
    {
        iofunc_func_init(&timeattrs[i].attr, S_IFCHR | 0666, NULL, NULL);
        timeattrs[i].attr.mount = &time_mount;
        timeattrs[i].format = formats[i];
        pathnameID[i] = resmgr_attach(dpp, &rattr, devnames[i], _FTYPE_ANY, 0, &connect_funcs, &io_funcs, &timeattrs[i]);

        if (pathnameID[i] == -1) 
        {
            fprintf(stderr, "%s: couldn't attach pathname: %s, errno: %d\n", programe, devnames[i], errno);
            exit(1);
        }
    } 

    ctp = dispatch_context_alloc(dpp);
    while (1)
    {
        if ((ctp = dispatch_block(ctp)) == NULL)
        {
            fprintf(stderr, "%s: dispatch_block failed: %s\n", programe, stderr(errno));
            exit(1);
        } 
        dispatch_handler(ctp);
    }
    
}


void options(int argc, char**argv)
{
    int opt;
    optv = 0;

    while ((opt = getopt(argc, argv, "v")) != -1)
    {
        switch (opt)
        {
        case 'v':
            opt = 1;
            break;
        
        default:
            break;
        }
    }
    
}

int io_read(resmgr_context_t *ctp, io_read_t *msg, Timeocb_t *tocb)
{
    int nleft;
    int onbytes;
    int status;

    if (optv) 
    {
        printf ("%s:  in io_read, offset is %lld, nbytes %d\n",
                progname, tocb -> ocb . offset, msg -> i.nbytes);
    }

    if((status == iofunc_read_verify(ctp, msg, &tocb->ocb, NULL)) != EOK)
    {
        return(status);
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return(ENOSYS);
    }

    if (tocb->buffer == NULL) 
    {
        tocb->buffer  = format_time(tocb->ocb.attr->format, 0);
        tocb->bufsize = strlen(tocb->buffer) + 1;
    }

    nleft = tocb->bufsize - tocb->ocb.offset;
    onbytes = min(msg->i.nbytes, nleft); 

    if (onbytes)
    {
        MsgReply(ctp->rcvid, onbytes, tocb->buffer + tocb->ocb.offset, onbytes);
    }
    else
    {
        MsgReply(ctp->rcvid, 0, NULL, 0);
    }

    tocb->ocb.offset += onbytes;
    if (msg->i.nbytes > 0)
    {
        tocb->ocb.attr->attr.flags |= IOFUNC_ATTR_ATIME;
    }

    return (_RESMGR_NOREPLY);
}

int io_write(resmgr_context_t *ctp, io_write_t *msg, Timeocb_t *tocb)
{
    int status;

    if (optv) {
        printf ("%s:  in io_write\n", progname);
    }

    if ((status == iofunc_write_verify(ctp, msg, &tocb->ocb, NULL)) ！= EOK)
    {
        return(status);
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) 
    {
        return(ENOSYS)
    }

    _IO_SET_WRITE_NBYTES(ctp, msg->i.nbytes);
    
    if (msg->i.nbytes > 0)
    {
        tocb->ocb.attr->attr.flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_CTIME;
    }

    return (_RESMGR_NPARTS(0));
}

Timeocb_t *time_ocb_calloc(resmgr_context_t *ctp, Timeattr_t *tattr)
{
    Timeocb_t *tocb;
    if (optv) 
    {
        printf("%s: in time_ocb_call \n", programe);
    }

    if ((tocb = calloc(1, sizeof(Timeocb_t))) == NULL)
    {
        if (optv) 
        {
            printf("%s: couldn't allocate %d bytes\n", programe, sizeof(Timeocb_t));
        }

        return (NULL);
    }

    tocb->buffer = NULL;

    return (tocb);
}

void time_ocb_free(Timeocb_t *tocb)
{
    if (optv) 
    {
        printf("%s: in time_ocb_free \n", programe);
    }

    if (tocb->buffer)
    {
        free(tocb->buffer);
    }

    free(tocb);
}

char *format_time(char *format, int time_offset)
{
    char *ptr;
    time_t now;
    ptr = malloc(64);
    time(&now);
    now += time_offset;
    strftime(ptr, 64, format, localtime(&now));
    return(ptr);
}

