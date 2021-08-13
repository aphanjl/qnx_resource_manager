#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>

static char *buffer = "Hello world\n";
int io_read(resmgr_context_t* ctp, io_read_t* msg, RESMGR_OCB_T* ocb);
int io_open(resmgr_context_t* ctp, io_open_t* msg, RESMGR_HANDLE_T *handle, void *extra);
int io_write(resmgr_context_t* ctp, io_write_t* msg, RESMGR_OCB_T* ocb);

static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t io_funcs;
static iofunc_attr_t attr;

int main(int argc, char **argv)
{
    resmgr_attr_t resmgr_attr;
    dispatch_t *dpp;
    dispatch_context_t *ctp;
    int id;

    // 1. 创建dispatch
    if ((dpp = dispatch_create()) == NULL) {
        fprintf(stderr, "%s: Unable to allocate dispatch handle. \n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // 2. 初始化resource manager 属性
    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 1;
    resmgr_attr.msg_max_size = 2048;

    // 3. 初始化消息处理函数
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS &io_funcs);
    connect_funcs.open = io_open;
    io_funcs.read  = io_read;
    io_funcs.write = io_write; 

    // 4. 初始化属性结构体
    iofunc_attr_init(&attr, S_IFNAM | 0666, 0, 0);

    // 5. attach 设备名
    id = resmgr_attach(
        dpp,
        &resmgr_attr,
        "/dev/sample",
        _FTYPE_ANY,
        0,
        &connect_funcs,
        &io_funcs,
        &attr);

    if (id == -1) 
    {
        fprintf(stderr, "%s: Unable to attach name. \n", argv[0]);
        return EXIT_FAILURE;
    }

    // 6. 创建上下文
    ctp = dispatch_context_alloc(dpp);

    // 7. 启动资源管理消息循环
    while (1)
    {
       if ((ctp = dispatch_block(ctp)) == NULL) {
           fprintf(stderr, "block error\n");
           return EXIT_FAILURE;
       }

       dispatch_handler(ctp);
    }
    
    return EXIT_SUCCESS;
}

int io_open(resmgr_context_t* ctp, io_open_t* msg, RESMGR_HANDLE_T *handle, void *extra)
{
    printf("got an open message\n");
    return iofunc_open_default(ctp, msg, handle, extra);
}

int io_read(resmgr_context_t* ctp, io_read_t* msg, RESMGR_OCB_T* ocb)
{
    int nleft;
    int nbytes;
    int nparts;
    int status;

    if ((status == iofunc_read_verify(ctp, msg, ocb, NULL)) != EOK) {
        return (status);
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return (ENOSYS);
    }

    nleft  = ocb->attr->nbytes - ocb->offset;
    nbytes = min(msg->i.nbytes, nleft);

    if (nbytes > 0)
    {   
        SETIOV(ctp->iov, buffer + ocb->offset, nbytes);
        _IO_SET_READ_NBYTES(ctp, nbytes);
        ocb->offset += nbytes;
        nparts = 1;
    }
    else 
    {
        _IO_SET_READ_NBYTES(ctp, 0);
        nparts = 0;
    }

    if (msg->i.nbytes > 0)
    {
        ocb->attr->flags |= IOFUNC_ATTR_ATIME;
    }

    return (_RESMGR_NPARTS(nparts));
}

int io_write(resmgr_context_t* ctp, io_write_t* msg, RESMGR_OCB_T* ocb)
{
    int status;
    if ((status = iofunc_write_verify(ctp, msg, ocb, NULL)) != EOK)
    {
        return status;
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        _IO_SET_WRITE_NBYTES(ctp, msg->i.nbytes);
    }

    if (msg->i.nbytes > 0)
    {
        ocb->attr->flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_CTIME;
    }

    return _RESMGR_NPARTS (0);
}