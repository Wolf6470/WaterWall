#include "shiftbuffer.h"
#include "wlibc.h"

// #define LEFTPADDING  ((RAM_PROFILE >= kRamProfileS2Memory ? (1U << 10) : (1U << 8)) - (sizeof(uint32_t) * 3))
// #define RIGHTPADDING ((RAM_PROFILE >= kRamProfileS2Memory ? (1U << 9) : (1U << 7)))

// #define TOTALPADDING ((uint32_t) (sizeof(sbuf_t) + (LEFTPADDING + RIGHTPADDING)))

void sbufDestroy(sbuf_t *b)
{
    memoryFree(b);
}

sbuf_t *sbufNewWithPad(uint32_t minimum_capacity, uint16_t pad_left, uint16_t pad_right)
{
    if (minimum_capacity != 0 && minimum_capacity % kCpuLineCacheSize != 0)
    {
        minimum_capacity = (max(kCpuLineCacheSize, minimum_capacity) + kCpuLineCacheSizeMin1) & ~kCpuLineCacheSizeMin1;
    }

    uint32_t real_cap = minimum_capacity + pad_left + pad_right;
    sbuf_t  *b        = memoryAllocate(real_cap);

    b->len      = 0;
    b->curpos   = pad_left;
    b->capacity = real_cap;
    b->l_pad    = pad_left;
    b->r_pad    = pad_right;

    return b;
}

sbuf_t *sbufNew(uint32_t minimum_capacity)
{
    return sbufNewWithPad(minimum_capacity, 0, 0);
}

sbuf_t *sbufDuplicate(sbuf_t *b)
{
    sbuf_t *newbuf = sbufNewWithPad(sbufCapNoPadding(b), b->l_pad, b->r_pad);
    sbufSetLength(newbuf, sbufGetBufLength(b));
#if MEM128_BUF_OPTIMIZE
    memoryCopy128(sbufGetMutablePtr(newbuf), sbufGetRawPtr(b), sbufGetBufLength(b));
#else
    memoryCopy(sbufGetMutablePtr(newbuf), sbufGetRawPtr(b), sbufGetBufLength(b));
#endif
    return newbuf;
}

sbuf_t *sbufConcat(sbuf_t *restrict root, const sbuf_t *restrict const buf)
{
    uint32_t root_length   = sbufGetBufLength(root);
    uint32_t append_length = sbufGetBufLength(buf);
    root                   = sbufReserveSpace(root, root_length + append_length);
    sbufSetLength(root, root_length + append_length);

    if (MEM128_BUF_OPTIMIZE && sbufGetRightCapacity(root) - append_length >= 128 && sbufGetRightCapacity(buf) >= 128)
    {
        memoryCopy128(sbufGetMutablePtr(root) + root_length, sbufGetRawPtr(buf), append_length);
    }
    else
    {
        memoryCopy(sbufGetMutablePtr(root) + root_length, sbufGetRawPtr(buf), append_length);
    }

    memoryCopy(sbufGetMutablePtr(root) + root_length, sbufGetRawPtr(buf), append_length);
    return root;
}

sbuf_t *sbufConcatNoPadding(sbuf_t *restrict root, const sbuf_t *restrict const buf)
{
    uint32_t root_length   = sbufGetBufLength(root);
    uint32_t append_length = sbufGetBufLength(buf);
    root                   = sbufReserveSpaceNoPadding(root, root_length + append_length);
    sbufSetLength(root, root_length + append_length);

    if (MEM128_BUF_OPTIMIZE && sbufGetRightCapacityNoPadding(root) - append_length >= 128 &&
        sbufGetRightCapacityNoPadding(buf) >= 128)
    {
        memoryCopy128(sbufGetMutablePtr(root) + root_length, sbufGetRawPtr(buf), append_length);
    }
    else
    {
        memoryCopy(sbufGetMutablePtr(root) + root_length, sbufGetRawPtr(buf), append_length);
    }

    // memoryCopy(sbufGetMutablePtr(root) + root_length, sbufGetRawPtr(buf), append_length);
    return root;
}

sbuf_t *sbufSliceTo(sbuf_t *restrict dest, sbuf_t *restrict source, const uint32_t bytes)
{
    assert(bytes <= sbufGetBufLength(source));
    assert(sbufGetBufLength(dest) == 0);

    dest = sbufReserveSpace(dest, bytes);
    sbufSetLength(dest, bytes);

    sbufWriteBuf(dest, source, bytes);

    sbufShiftRight(source, bytes);

    return dest;
}

sbuf_t *sbufSlice(sbuf_t *const b, const uint32_t bytes)
{
    sbuf_t *newbuf = sbufNewWithPad(sbufCapNoPadding(b), b->l_pad, b->r_pad);
    sbufSliceTo(newbuf, b, bytes);
    return newbuf;
}
