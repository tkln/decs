#ifndef SB_H
#define SB_H

#include <stddef.h>
#include <stdlib.h>

#define container_of(container_type, node_member, node_ptr) \
        ((container_type *)((char *)(node_ptr) - \
            offsetof(container_type, node_member)))

struct sb_hdr {
    size_t sz;
    size_t cap;
    char data[0];
};

#define sb_size(buf) (!(buf) ? 0 : sb_hdr(buf)->sz)
#define sb_cap(buf) (!(buf) ? 0 : sb_hdr(buf)->cap)
#define sb_alloc_last(buf) (((buf = sb_grown(buf)), &buf[sb_hdr(buf)->sz++]))
#define sb_push(buf, obj) (*sb_alloc_last(buf) = (obj))
#define sb_last(buf) ((buf)[sb_hdr(buf)->sz - 1])
#define sb_alloc(buf, n) (sb_do_alloc(buf, n, sizeof(buf[0])))
#define sb_free(buf) (free(sb_hdr(buf)))

#define sb_hdr(buf) ((buf) ? container_of(struct sb_hdr, data, buf) : NULL)
#define sb_need_grow(buf) (!(buf) || sb_hdr(buf)->sz >= sb_hdr(buf)->cap)
#define sb_grown(buf) \
    (sb_need_grow(buf) ? sb_grow(buf, sizeof(buf[0])) : (buf)) 

static void *sb_do_alloc(void *buf, size_t n, size_t sz)
{
    struct sb_hdr *hdr;
    size_t total_sz = sizeof(struct sb_hdr) + n * sz;

    if (!buf) {
        hdr = malloc(total_sz);
        hdr->sz = 0;
    } else {
        hdr = realloc(sb_hdr(buf), total_sz);
    }

    hdr->cap = n;

    return hdr->data;
}

static void *sb_grow(void *buf, size_t sz)
{
    size_t n = buf ? 2 * sb_cap(buf) : 1;

    return sb_do_alloc(buf, n, sz);
}

#endif
