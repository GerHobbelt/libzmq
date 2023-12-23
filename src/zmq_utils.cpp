/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"

#include "macros.hpp"
#include "clock.hpp"
#include "err.hpp"
#include "thread.hpp"
#include "atomic_counter.hpp"
#include "atomic_ptr.hpp"
#include "random.hpp"
#include <assert.h>
#include <new>

#ifdef ZMQ_HAVE_TBB_SCALABLE_ALLOCATOR
#include <tbb/scalable_allocator.h>
#endif

#if !defined ZMQ_HAVE_WINDOWS
#include <unistd.h>
#endif

#if defined(ZMQ_USE_LIBSODIUM)
#include "sodium.h"
#endif

ZMQ_EXPORT_VOID_IMPL zmq_sleep (int seconds_)
{
#if defined ZMQ_HAVE_WINDOWS
    Sleep (seconds_ * 1000);
#else
    sleep (seconds_);
#endif
}

ZMQ_EXPORT_PTR_IMPL (void *, uint64_t) zmq_stopwatch_start (void)
{
#ifdef ZMQ_HAVE_TBB_SCALABLE_ALLOCATOR
    uint64_t *watch = static_cast<uint64_t *> (scalable_malloc (sizeof (uint64_t)));
#else
    uint64_t *watch = static_cast<uint64_t *> (std::malloc (sizeof (uint64_t)));
#endif
    alloc_assert (watch);
    *watch = zmq::clock_t::now_us ();
    return static_cast<void *> (watch);
}

ZMQ_EXPORT_IMPL (unsigned long) zmq_stopwatch_intermediate (_In_ void *watch_)
{
    const uint64_t end = zmq::clock_t::now_us ();
    const uint64_t start = *static_cast<uint64_t *> (watch_);
    return static_cast<unsigned long> (end - start);
}

ZMQ_EXPORT_IMPL (unsigned long)
zmq_stopwatch_stop (_In_ _Post_invalid_ void *watch_)
{
    const unsigned long res = zmq_stopwatch_intermediate (watch_);
#ifdef ZMQ_HAVE_TBB_SCALABLE_ALLOCATOR
    scalable_free (watch_);
#else
    std::free (watch_);
#endif
    return res;
}

ZMQ_EXPORT_VOID_PTR_IMPL
zmq_threadstart (_In_ zmq_thread_fn *func_, _In_opt_ void *arg_)
{
    zmq::thread_t *thread = new (std::nothrow) zmq::thread_t;
    alloc_assert (thread);
    thread->start (func_, arg_, "ZMQapp");
    return thread;
}

ZMQ_EXPORT_VOID_IMPL zmq_threadclose (_In_ _Post_invalid_ void *thread_)
{
    zmq::thread_t *p_thread = static_cast<zmq::thread_t *> (thread_);
    p_thread->stop ();
    LIBZMQ_DELETE (p_thread);
}

//  Z85 codec, taken from 0MQ RFC project, implements RFC32 Z85 encoding

//  Maps base 256 to base 85
static char encoder[85 + 1] = {"0123456789"
                               "abcdefghij"
                               "klmnopqrst"
                               "uvwxyzABCD"
                               "EFGHIJKLMN"
                               "OPQRSTUVWX"
                               "YZ.-:+=^!/"
                               "*?&<>()[]{"
                               "}@%$#"};

//  Maps base 85 to base 256
//  We chop off lower 32 and higher 128 ranges
//  0xFF denotes invalid characters within this range
static uint8_t decoder[96] = {
  0xFF, 0x44, 0xFF, 0x54, 0x53, 0x52, 0x48, 0xFF, 0x4B, 0x4C, 0x46, 0x41,
  0xFF, 0x3F, 0x3E, 0x45, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x40, 0xFF, 0x49, 0x42, 0x4A, 0x47, 0x51, 0x24, 0x25, 0x26,
  0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
  0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x4D,
  0xFF, 0x4E, 0x43, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
  0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x4F, 0xFF, 0x50, 0xFF, 0xFF};

//  --------------------------------------------------------------------------
//  Encode a binary frame as a string; destination string MUST be at least
//  size * 5 / 4 bytes long plus 1 byte for the null terminator. Returns
//  dest. Size must be a multiple of 4.
//  Returns NULL and sets errno = EINVAL for invalid input.

ZMQ_EXPORT_STR_SIZE_IMPL (char *, size_ * 4 / 5 + 1)
zmq_z85_encode (_Out_writes_z_ (size_ * 4 / 5 + 1) char *dest_,
                _In_reads_bytes_ (size_) const uint8_t *data_,
                size_t size_)
{
    if (size_ % 4 != 0) {
        errno = EINVAL;
        return NULL;
    }
    unsigned int char_nbr = 0;
    unsigned int byte_nbr = 0;
    uint32_t value = 0;
    while (byte_nbr < size_) {
        //  Accumulate value in base 256 (binary)
        value = value * 256 + data_[byte_nbr++];
        if (byte_nbr % 4 == 0) {
            //  Output value in base 85
            unsigned int divisor = 85 * 85 * 85 * 85;
            while (divisor) {
                dest_[char_nbr++] = encoder[value / divisor % 85];
                divisor /= 85;
            }
            value = 0;
        }
    }
    assert (char_nbr == size_ * 5 / 4);
    dest_[char_nbr] = 0;
    return dest_;
}


//  --------------------------------------------------------------------------
//  Decode an encoded string into a binary frame; dest must be at least
//  strlen (string) * 4 / 5 bytes long. Returns dest. strlen (string)
//  must be a multiple of 5.
//  Returns NULL and sets errno = EINVAL for invalid input.

ZMQ_EXPORT_BUF_SIZE (uint8_t *, _String_length_ (string_) * 4 / 5)
zmq_z85_decode (_Out_writes_bytes_ (_String_length_ (string_) * 4 / 5)
                  uint8_t *dest_,
                _In_z_ const char *string_)
{
    unsigned int byte_nbr = 0;
    unsigned int char_nbr = 0;
    uint32_t value = 0;
    size_t src_len = strlen (string_);

    if (src_len < 5 || src_len % 5 != 0)
        goto error_inval;

    while (string_[char_nbr]) {
        //  Accumulate value in base 85
        if (UINT32_MAX / 85 < value) {
            //  Invalid z85 encoding, represented value exceeds 0xffffffff
            goto error_inval;
        }
        value *= 85;
        const uint8_t index = string_[char_nbr++] - 32;
        if (index >= sizeof (decoder)) {
            //  Invalid z85 encoding, character outside range
            goto error_inval;
        }
        const uint32_t summand = decoder[index];
        if (summand == 0xFF || summand > (UINT32_MAX - value)) {
            //  Invalid z85 encoding, invalid character or represented value exceeds 0xffffffff
            goto error_inval;
        }
        value += summand;
        if (char_nbr % 5 == 0) {
            //  Output value in base 256
            unsigned int divisor = 256 * 256 * 256;
            while (divisor) {
                dest_[byte_nbr++] = value / divisor % 256;
                divisor /= 256;
            }
            value = 0;
        }
    }
    if (char_nbr % 5 != 0) {
        goto error_inval;
    }
    assert (byte_nbr == strlen (string_) * 4 / 5);
    return dest_;

error_inval:
    errno = EINVAL;
    return NULL;
}

//  --------------------------------------------------------------------------
//  Generate a public/private keypair with libsodium.
//  Generated keys will be 40 byte z85-encoded strings.
//  Returns 0 on success, -1 on failure, setting errno.
//  Sets errno = ENOTSUP in the absence of a CURVE library.

ZMQ_EXPORT_IMPL (int)
zmq_curve_keypair (_Out_writes_z_ (41) char *z85_public_key_,
                   _Out_writes_z_ (41) char *z85_secret_key_)
{
#if defined(ZMQ_HAVE_CURVE)
#if crypto_box_PUBLICKEYBYTES != 32 || crypto_box_SECRETKEYBYTES != 32
#error "CURVE encryption library not built correctly"
#endif

    uint8_t public_key[32];
    uint8_t secret_key[32];

    zmq::random_open ();

    const int res = crypto_box_keypair (public_key, secret_key);
    (void) zmq_z85_encode (z85_public_key_, public_key, 32);
    (void) zmq_z85_encode (z85_secret_key_, secret_key, 32);

    zmq::random_close ();

    return res;
#else
    (void) z85_public_key_, (void) z85_secret_key_;
    errno = ENOTSUP;
    return -1;
#endif
}

//  --------------------------------------------------------------------------
//  Derive the public key from a private key using libsodium.
//  Derived key will be 40 byte z85-encoded string.
//  Returns 0 on success, -1 on failure, setting errno.
//  Sets errno = ENOTSUP in the absence of a CURVE library.

ZMQ_EXPORT_IMPL (int)
zmq_curve_public (_Out_writes_z_ (41) char *z85_public_key_,
                  _In_reads_z_ (41) const char *z85_secret_key_)
{
#if defined(ZMQ_HAVE_CURVE)
#if crypto_box_PUBLICKEYBYTES != 32 || crypto_box_SECRETKEYBYTES != 32
#error "CURVE encryption library not built correctly"
#endif

    uint8_t public_key[32];
    uint8_t secret_key[32];

    zmq::random_open ();

    if (zmq_z85_decode (secret_key, z85_secret_key_) == NULL)
        return -1;

    // Return codes are suppressed as none of these can actually fail.
    crypto_scalarmult_base (public_key, secret_key);
    zmq_z85_encode (z85_public_key_, public_key, 32);

    zmq::random_close ();

    return 0;
#else
    (void) z85_public_key_, (void) z85_secret_key_;
    errno = ENOTSUP;
    return -1;
#endif
}


//  --------------------------------------------------------------------------
//  Initialize a new atomic counter, which is set to zero

ZMQ_EXPORT_VOID_PTR_IMPL zmq_atomic_counter_new (void)
{
    zmq::atomic_counter_t *counter = new (std::nothrow) zmq::atomic_counter_t;
    alloc_assert (counter);
    return counter;
}

//  Se the value of the atomic counter

ZMQ_EXPORT_VOID_IMPL zmq_atomic_counter_set (_Inout_ void *counter_, int value_)
{
    (static_cast<zmq::atomic_counter_t *> (counter_))->set (value_);
}

//  Increment the atomic counter, and return the old value

ZMQ_EXPORT_IMPL (int) zmq_atomic_counter_inc (_Inout_ void *counter_)
{
    return (static_cast<zmq::atomic_counter_t *> (counter_))->add (1);
}

//  Decrement the atomic counter and return 1 (if counter >= 1), or
//  0 if counter hit zero.

ZMQ_EXPORT_IMPL (int) zmq_atomic_counter_dec (_Inout_ void *counter_)
{
    return (static_cast<zmq::atomic_counter_t *> (counter_))->sub (1) ? 1 : 0;
}

//  Return actual value of atomic counter

ZMQ_EXPORT_IMPL (int) zmq_atomic_counter_value (_In_ void *counter_)
{
    return (static_cast<zmq::atomic_counter_t *> (counter_))->get ();
}

//  Destroy atomic counter, and set reference to NULL

ZMQ_EXPORT_VOID_IMPL
zmq_atomic_counter_destroy (_Inout_ _Deref_post_null_ void **counter_p_)
{
    delete (static_cast<zmq::atomic_counter_t *> (*counter_p_));
    *counter_p_ = NULL;
}
