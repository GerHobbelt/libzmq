/* SPDX-License-Identifier: MPL-2.0 */

#include "../include/zmq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.hpp"

#if defined ZMQ_HAVE_WINDOWS
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

static size_t message_size;
static int roundtrip_count;

#if defined ZMQ_HAVE_WINDOWS
static unsigned int __stdcall worker (void *ctx_)
#else
static void *worker (void *ctx_)
#endif
{
    void *s;
    int rc;
    int i;
    zmq_msg_t msg;

    s = zmq_socket (ctx_, ZMQ_REP);
    if (!s) {
        printf ("error in zmq_socket: %s\n", zmq_strerror (errno));
        exit (1);
    }

    rc = zmq_connect (s, "inproc://lat_test");
    if (rc != 0) {
        printf ("error in zmq_connect: %s\n", zmq_strerror (errno));
        exit (1);
    }

    rc = zmq_msg_init (&msg);
    if (rc != 0) {
        printf ("error in zmq_msg_init: %s\n", zmq_strerror (errno));
        exit (1);
    }

    for (i = 0; i != roundtrip_count; i++) {
        rc = zmq_recvmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_recvmsg: %s\n", zmq_strerror (errno));
            exit (1);
        }
        rc = zmq_sendmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_sendmsg: %s\n", zmq_strerror (errno));
            exit (1);
        }
    }

    rc = zmq_msg_close (&msg);
    if (rc != 0) {
        printf ("error in zmq_msg_close: %s\n", zmq_strerror (errno));
        exit (1);
    }

    rc = zmq_close (s);
    if (rc != 0) {
        printf ("error in zmq_close: %s\n", zmq_strerror (errno));
        exit (1);
    }

#if defined ZMQ_HAVE_WINDOWS
    return 0;
#else
    return NULL;
#endif
}


#if defined(BUILD_MONOLITHIC)
#define main        zmq_perf_inproc_lat_main
#endif

int main(int argc, const char **argv)
{
#if defined ZMQ_HAVE_WINDOWS
    HANDLE local_thread;
#else
    pthread_t local_thread;
#endif
    void *ctx;
    void *s;
    int rc;
    int i;
    zmq_msg_t msg;
    void *watch;
    unsigned long elapsed;
    double latency;

    if (argc != 3) {
        printf ("usage: inproc_lat <message-size> <roundtrip-count>\n");
        return 1;
    }

    message_size = atoi (argv[1]);
    roundtrip_count = atoi (argv[2]);

    ctx = zmq_init (1);
    if (!ctx) {
        printf ("error in zmq_init: %s\n", zmq_strerror (errno));
        return -1;
    }

    s = zmq_socket (ctx, ZMQ_REQ);
    if (!s) {
        printf ("error in zmq_socket: %s\n", zmq_strerror (errno));
        return -1;
    }

    rc = zmq_bind (s, "inproc://lat_test");
    if (rc != 0) {
        printf ("error in zmq_bind: %s\n", zmq_strerror (errno));
        return -1;
    }

#if defined ZMQ_HAVE_WINDOWS
    local_thread = (HANDLE) _beginthreadex (NULL, 0, worker, ctx, 0, NULL);
    if (local_thread == 0) {
        printf ("error in _beginthreadex\n");
        return -1;
    }
#else
    rc = pthread_create (&local_thread, NULL, worker, ctx);
    if (rc != 0) {
        printf ("error in pthread_create: %s\n", zmq_strerror (rc));
        return -1;
    }
#endif

    rc = zmq_msg_init_size (&msg, message_size);
    if (rc != 0) {
        printf ("error in zmq_msg_init_size: %s\n", zmq_strerror (errno));
        return -1;
    }
    memset (zmq_msg_data (&msg), 0, message_size);

    printf ("message size: %d [B]\n", (int) message_size);
    printf ("roundtrip count: %d\n", (int) roundtrip_count);

    watch = zmq_stopwatch_start ();

    for (i = 0; i != roundtrip_count; i++) {
        rc = zmq_sendmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_sendmsg: %s\n", zmq_strerror (errno));
            return -1;
        }
        rc = zmq_recvmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_recvmsg: %s\n", zmq_strerror (errno));
            return -1;
        }
        if (zmq_msg_size (&msg) != message_size) {
            printf ("message of incorrect size received\n");
            return -1;
        }
    }

    elapsed = zmq_stopwatch_stop (watch);

    rc = zmq_msg_close (&msg);
    if (rc != 0) {
        printf ("error in zmq_msg_close: %s\n", zmq_strerror (errno));
        return -1;
    }

    latency = (double) elapsed / (roundtrip_count * 2);

#if defined ZMQ_HAVE_WINDOWS
    DWORD rc2 = WaitForSingleObject (local_thread, INFINITE);
    if (rc2 == WAIT_FAILED) {
        printf ("error in WaitForSingleObject\n");
        return -1;
    }
    BOOL rc3 = CloseHandle (local_thread);
    if (rc3 == 0) {
        printf ("error in CloseHandle\n");
        return -1;
    }
#else
    rc = pthread_join (local_thread, NULL);
    if (rc != 0) {
        printf ("error in pthread_join: %s\n", zmq_strerror (rc));
        return -1;
    }
#endif

    printf ("average latency: %.3f [us]\n", (double) latency);

    rc = zmq_close (s);
    if (rc != 0) {
        printf ("error in zmq_close: %s\n", zmq_strerror (errno));
        return -1;
    }

    rc = zmq_ctx_term (ctx);
    if (rc != 0) {
        printf ("error in zmq_ctx_term: %s\n", zmq_strerror (errno));
        return -1;
    }

    return 0;
}
