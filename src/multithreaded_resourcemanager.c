#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>

#define THREAD_POOL_PARAM_T

static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t      io_funcs;
static iofunc_attr_init       attr;

static char *buffer = "Hello world\n";
int io_read(resmgr_context_t* ctp, io_read_t* msg, RESMGR_OCB_T* ocb);
int io_open(resmgr_context_t* ctp, io_open_t* msg, RESMGR_HANDLE_T *handle, void *extra);
int io_write(resmgr_context_t* ctp, io_write_t* msg, RESMGR_OCB_T* ocb);

int main(int argc, char**argv)
{
    thread_pool_attr_t pool_attr;
    resmgr_attr_t      resmgr_attr;
    dispatch_t         *dpp;
    thread_pool_t      *tpp;
    int                id;

    // 1. 创建dispatch
    if ((dpp = dispatch_create()) == NULL)
    {
        fprintf(stderr, "%s: Unable to allocate dispatch handle. \n", argv[0]);
        return EXIT_FAILURE;
    }

    // 2. 初始化资源管理属性
    memset(&resmgr_attr, 0, sizeof(resmgr_attr));

    // 3. 初始化消息处理函数
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
    connect_funcs.open = io_open;
    io_funcs.read  = io_read;
    io_funcs.write = io_write;

    // 4. attach 设备
    id = resmgr_attach(dpp
                       &resmgr_attr，
                       “/dev/sample”,
                       _FTYPE_ANY,
                       0,
                       &connect_funcs,
                       &io_funcs,
                       &attr)；
    if (id == -1) {
        fprintf(stderr, "%s: Unable to attach name. \n", argv[0]);
        return EXIT_FAILURE;
    }

    // 5. 初始化线程池属性
    memset(&pool_attr, 0, sizeof(pool_attr));
    pool_attr.handle = dpp;
    pool_attr.context_alloc = dispatch_context_alloc;
    pool_attr.block_func    = dispatch_block;
    pool_attr.unblock_func  = dispatch_unblock;
    pool_attr.context_free  = dispatch_context_free;
    pool_attr.lo_water  = 2;
    pool_attr.hi_water  = 4;
    pool_attr.increment = 1;
    pool_attr.maximum   = 50;

    // 6. 创建线程池
    if((tpp = thread_pool_create(&pool_attr, POOL_FLAG_EXIT_SELF)) == NULL) {
        fprintf(stderr, "%s: Unable to initialize thread pool. \n", argv[0]);
        return EXIT_FAILURE;
    }

    // 7. 启动线程
    thread_pool_start(tpp);
    return EXIT_FAILURE;

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