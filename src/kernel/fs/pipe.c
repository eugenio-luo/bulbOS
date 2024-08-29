#include <string.h>
#include <stdlib.h>

#include <stdio.h>

#include <kernel/pipe.h>
#include <kernel/file.h>
#include <kernel/mutex.h>
#include <kernel/vmm.h>

int
pipe_buf_write(pipe_t *pipe, void *addr, size_t size)
{
        if (pipe->b_read != pipe->b_write)
                condvar_wait(&pipe->b_write, pipe->mutex);
        
        size_t remaining = size;
        if (size > pipe->size - (pipe->b_write % pipe->size))
                remaining = pipe->size - (pipe->b_write % pipe->size);

        memcpy(addr, pipe->buffer, remaining);
        pipe->b_write += remaining;
        return (int) remaining;
}
        
int
pipe_unbuf_write(pipe_t *pipe, void *addr, size_t size)
{
        size_t i = 0;
        for (; i < size; ++i) {
                while (pipe->b_write == pipe->b_read + PIPE_SIZE) {
                        if (!pipe->read_open) {
                                mutex_release(pipe->mutex);
                                return -1;
                        }
                        
                        condvar_signal(&pipe->b_read);
                        condvar_wait(&pipe->b_write, pipe->mutex);
                }

                pipe->buffer[pipe->b_write++ % PIPE_SIZE] = ((char*)addr)[i];
        }

        return (int) i;
}

int
pipe_write(file_t *file, void *addr, size_t size)
{
        pipe_t *pipe = file->pipe;
        mutex_acquire(pipe->mutex);

        int ret = -1;
        switch (pipe->type) {
        case PIPE_TYPE_BUF_CHAR:
        case PIPE_TYPE_UNBUFFERED:
                ret = pipe_unbuf_write(pipe, addr, size);
                break;
                
        case PIPE_TYPE_BUF_BLOCK:
                ret = pipe_buf_write(pipe, addr, size);
                break;
        default:
                break;
        }

        if (pipe->type != PIPE_TYPE_BUF_BLOCK || !(pipe->b_write % pipe->size))
                condvar_signal(&pipe->b_read);
        
        mutex_release(pipe->mutex);
        return ret;
}

static int
pipe_bufblock_read(pipe_t *pipe, void *addr, size_t size)
{
        if (size != pipe->size)
                return -1;

        if (pipe->b_read + size != pipe->b_write && pipe->write_open)
                condvar_wait(&pipe->b_read, pipe->mutex);

        memcpy(addr, pipe->buffer, size);
        pipe->b_read += size;
        return size;
}

static int
pipe_bufchar_read(pipe_t *pipe, void *addr, char c)
{
        if (pipe->b_read == pipe->b_write && pipe->write_open)
                condvar_wait(&pipe->b_read, pipe->mutex);
        
        int i = 0;
        for (;;) {
                if (pipe->b_read == pipe->b_write)
                        break;
                        
                ((char*)addr)[i++] = pipe->buffer[pipe->b_read % pipe->size];

                if (pipe->buffer[pipe->b_read++ % pipe->size] == c)
                        break;
        }

        return i;
}

static int
pipe_unbuf_read(pipe_t *pipe, void *addr, size_t size)
{
        if (pipe->b_read == pipe->b_write && pipe->write_open)
                condvar_wait(&pipe->b_read, pipe->mutex);

        size_t i = 0;
        for (; i < size; ++i) {
                if (pipe->b_read == pipe->b_write)
                        break;

                ((char*)addr)[i] = pipe->buffer[pipe->b_read++ % pipe->size];
        }

        return (int) i;
}

int
pipe_read(file_t *file, void *addr, size_t size)
{
        pipe_t *pipe = file->pipe;
        mutex_acquire(pipe->mutex);

        if (!pipe->write_open) {
                mutex_release(pipe->mutex);
                return -1;
        }

        int ret = -1;
        switch (pipe->type) {
        case PIPE_TYPE_UNBUFFERED:
                ret = pipe_unbuf_read(pipe, addr, size);
                break;
                
        case PIPE_TYPE_BUF_BLOCK:
                ret = pipe_bufblock_read(pipe, addr, size);
                break;

        case PIPE_TYPE_BUF_CHAR: {
                char c = (char) size;
                ret = pipe_bufchar_read(pipe, addr, c);
                break;
        }
        default:
                break;
        }

        condvar_signal(&pipe->b_write);
        mutex_release(pipe->mutex);
        return ret;
}

void
pipe_close(file_t *file)
{
        pipe_t *pipe = file->pipe;
        mutex_acquire(pipe->mutex);

        if (file->writep) {
                pipe->write_open = 0;
                condvar_signal(&pipe->b_read);
        } else {
                pipe->read_open = 0;
        }

        mutex_release(pipe->mutex);
        if (!pipe->write_open && !pipe->read_open) {
                kfree(pipe->mutex);
                kfree(pipe->buffer);
                condvar_free(&pipe->b_read);
                condvar_free(&pipe->b_write);
                kfree(pipe);
        }
}

int
pipe_alloc(file_t **f_read, file_t **f_write, uint32_t size, pipe_type_t type)
{
        *f_read = file_alloc();
        if (!*f_read)
                return -1;
        
        *f_write = file_alloc();
        if (!*f_write) {
                file_close(*f_read);
                return -1;
        }
        
        pipe_t *pipe = kmalloc(sizeof(pipe_t));
        pipe->type = type;
        pipe->size = size;
        pipe->read_open = 1;
        pipe->write_open = 1;
        pipe->b_write = 0;
        pipe->b_read = 0;
        pipe->buffer = kmalloc(size);
        pipe->mutex = mutex_create();

        (*f_read)->readp = 1;
        (*f_read)->writep = 0;
        (*f_read)->pipe = pipe;
        (*f_read)->read = pipe_read;
        (*f_read)->close = pipe_close;

        (*f_write)->readp = 0;
        (*f_write)->writep = 1;
        (*f_write)->pipe = pipe;
        (*f_write)->write = pipe_write;
        (*f_write)->close = pipe_close;
        return 0;
}
