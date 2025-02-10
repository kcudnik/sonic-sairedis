#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>

#include <vat/vat.h>
#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include <vppinfra/error.h>

#include <vpp_plugins/af_packet/af_packet.h>
#include <vpp_plugins/af_packet/af_packet.api_types.h>
#include <vpp_plugins/af_packet/af_packet.api_enum.h>
#include <vlibapi/vat_helper_macros.h>

#include <svm/svm_common.h>

#include "xlate/SaiVppXlate.h"

#define INTERFACE_NAME "vpp1out"
#define VPP_SOCKET_PATH "/run/vpp/api1.sock"

// compile: gcc main.c -o main -lvlib -lvlibapi -lvppapiclient -lvlibmemoryclient -lvppinfra
// vpp v25.02-rc0~169-gf0a126a1e built by kcudnik on ubuntu2 at 2024-12-09T16:08:56

vat_main_t vat_main;

// TODO hack, where is this defined ?
#define __plugin_msg_base 0

/*
// TODO this needs to be defined to link, why?
f64 vat_time_now (vat_main_t * vam)
{
#if VPP_API_TEST_BUILTIN
  return vlib_time_now (vam->vlib_main);
#else
  return clib_time_now (&vam->clib_time);
#endif
}

// TODO this needs to be defined to link, why?
void __clib_no_tail_calls vat_suspend (vlib_main_t *vm, f64 interval)
{
    const struct timespec req = {0, 100000000};
    nanosleep(&req, NULL);
}
*/

static void scenario1()
{
    // TODO THIS crashes on
    // clib_mem_heap_alloc_inline (heap=<optimized out>, size=9, align=8, os_out_of_memory_on_failure=1)
    vl_socket_client_connect(VPP_SOCKET_PATH, "myclient", 0);
}

static void scenario2()
{
    clib_mem_init_thread_safe(0, 128 << 20);

    int ret = vl_socket_client_connect(VPP_SOCKET_PATH, "myclient", 0);

    if (ret < 0)
    {
        fprintf(stderr, "Failed to connect to VPP socket: %s\n", VPP_SOCKET_PATH);
        fprintf(stderr, "Error Code: %d, Error Message: %s\n", errno, strerror(errno));
        exit(1);
    }

    // TODO THIS crashes on:
    // 0  0x00007f7a2f0e414f in vl_msg_api_alloc (nbytes=15) at /home/kcudnik/vpp/repo/vpp/src/vlibapi/memory_shared.c:210
    // 210       pool = (am->our_pid == shmem_hdr->vl_pid);
    vl_client_disconnect();
}

static int vl_socket_client_read_internal (socket_client_main_t * scm, int wait)
{
    u32 data_len = 0, msg_size;
    int n, current_rx_index;
    msgbuf_t *mbp = 0;
    f64 timeout;

    if (scm->socket_fd == 0)
        return -1;

    if (wait)
        timeout = clib_time_now (&scm->clib_time) + wait;

    while (1)
    {
        printf("1"); fflush(stdout);
        current_rx_index = vec_len (scm->socket_rx_buffer);
        while (current_rx_index < sizeof (*mbp))
        {
            printf("2: len %d, ", current_rx_index); fflush(stdout);
            vec_validate (scm->socket_rx_buffer, current_rx_index
                    + scm->socket_buffer_size - 1);
            n = read (scm->socket_fd, scm->socket_rx_buffer + current_rx_index,
                    scm->socket_buffer_size);

            printf("read: %d: errno: %d: %s\n", n, errno, strerror(errno));
            if (n < 0)
            {
                if (errno == EAGAIN)
                    continue;

                clib_unix_warning ("socket_read");
                vec_set_len (scm->socket_rx_buffer, current_rx_index);
                return -1;
            }
            current_rx_index += n;
        }

        printf("3\n");
        vec_set_len (scm->socket_rx_buffer, current_rx_index);

#if CLIB_DEBUG > 1
        if (n > 0)
            clib_warning ("read %d bytes", n);
#endif

        mbp = (msgbuf_t *) (scm->socket_rx_buffer);
        data_len = ntohl (mbp->data_len);
        current_rx_index = vec_len (scm->socket_rx_buffer);
        vec_validate (scm->socket_rx_buffer, current_rx_index + data_len);
        mbp = (msgbuf_t *) (scm->socket_rx_buffer);
        msg_size = data_len + sizeof (*mbp);

        while (current_rx_index < msg_size)
        {

            printf("4\n");
            n = read (scm->socket_fd, scm->socket_rx_buffer + current_rx_index,
                    msg_size - current_rx_index);
            if (n < 0)
            {
                if (errno == EAGAIN)
                    continue;

                clib_unix_warning ("socket_read");
                vec_set_len (scm->socket_rx_buffer, current_rx_index);
                return -1;
            }
            current_rx_index += n;
        }

        printf("5\n");
        vec_set_len (scm->socket_rx_buffer, current_rx_index);

        if (vec_len (scm->socket_rx_buffer) >= data_len + sizeof (*mbp))
        {
            vl_msg_api_socket_handler ((void *) (mbp->data), data_len);

            if (vec_len (scm->socket_rx_buffer) == data_len + sizeof (*mbp))
                vec_set_len (scm->socket_rx_buffer, 0);
            else
                vec_delete (scm->socket_rx_buffer, data_len + sizeof (*mbp), 0);
            mbp = 0;

            /* Quit if we're out of data, and not expecting a ping reply */
            if (vec_len (scm->socket_rx_buffer) == 0
                    && scm->control_pings_outstanding == 0)
                break;
        }
        if (wait && clib_time_now (&scm->clib_time) >= timeout)
            return -1;
    }
    return 0;
}

int set_socket_blocking_mode(int socket_fd) {
    // Get current flags of the socket
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        return -1;
    }

    // Remove the non-blocking flag if it exists
    flags &= ~O_NONBLOCK;

    // Set the socket to blocking mode
    if (fcntl(socket_fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL failed");
        return -1;
    }

    printf("blockin success\n");

    return 0;
}

static void scenario3()
{
}

static int
vsc_socket_connect (vat_main_t * vam, char *client_name)
{
    int rv;
    api_main_t *am = vlibapi_get_main ();
    vam->socket_client_main = &socket_client_main;
    if ((rv = vl_socket_client_connect ((char *) vam->socket_name,
                                        client_name,
                                        0 /* default socket rx, tx buffer */ )))
        return rv;

    /* vpp expects the client index in network order */
    vam->my_client_index = htonl (socket_client_main.client_index);
    am->my_client_index = vam->my_client_index;
    return 0;
}

int main()
{
    clib_mem_init_thread_safe(0, 128 << 20);

    api_main_t *am = vlibapi_get_main();
    vat_main_t *vam = &vat_main;

    vam->socket_client_main = &socket_client_main; // global

    int ret = vl_socket_client_connect(VPP_SOCKET_PATH, "myclient", 0);

    if (ret < 0)
    {
        fprintf(stderr, "Failed to connect to VPP socket: %s\n", VPP_SOCKET_PATH);
        fprintf(stderr, "Error Code: %d, Error Message: %s\n", errno, strerror(errno));
        exit(1);
    }

    // vpp expects the client index in network order
    vam->my_client_index = htonl (socket_client_main.client_index);
    am->my_client_index = vam->my_client_index;



//    {
//        vl_api_show_version_t *mp;
//        mp = vl_msg_api_alloc(sizeof(*mp));
//        memset(mp, 0, sizeof(*mp));
//        mp->_vl_msg_id = htons(VL_API_SHOW_VERSION);
//        mp->client_index = vam->my_client_index;
//        vl_socket_client_write(sockfd, mp, sizeof(*mp));
//        vl_socket_client_read(sockfd, response, sizeof(response), 1);
//    }

    // manually create host interface

//    svm_region_init();
//
//    svm_region_t * root_rp = svm_get_root_rp();
//    printf("root_rp: %p\n", root_rp);

//#define VPP_SHM_PATH "/run/vpp/api-shm"
//#define VPP_SHM_PATH "vpp1"
//    // allocate shared memory
//    ret = vl_map_shmem(VPP_SHM_PATH, 1);
//
//    if (ret != 0)
//    {
//        printf("memory failed shm\n");
//        exit(3);
//    }

    socket_client_main_t *scm = vam->socket_client_main;

    if (!scm)
    {
        printf("error no SCM!\n");
        exit(2);
    }

    set_socket_blocking_mode(scm->socket_fd);

    int flags = fcntl(scm->socket_fd, F_GETFL, 0);

    if (flags & O_NONBLOCK)
        printf("Socket is in NON-BLOCKING mode\n");
    else
        printf("Socket is BLOCKING mode\n");

    scm->socket_enable = 1;

    vl_socket_client_enable_disable2(scm,1);

    if (scm && scm->socket_enable)
        printf("* socked enabled\n");
    else
        printf("ERROR: socket not enabled! SHM will be used!\n");

    vl_api_af_packet_create_t *mp;

    printf("M\n");

    if (0)
    {
       M(AF_PACKET_CREATE, mp);
    }
    else
    {
        /* M: construct, but don't yet send a message */
        vam->result_ready = 0;
        mp = vl_socket_client_msg_alloc (sizeof(*mp));
        clib_memset (mp, 0, sizeof (*mp));
        mp->_vl_msg_id = ntohs (VL_API_AF_PACKET_CREATE+__plugin_msg_base);
        mp->client_index = vam->my_client_index;
    }

    u32 name_len = strlen(INTERFACE_NAME);

    // Copy interface name into API message
    strncpy((char *)mp->host_if_name, INTERFACE_NAME, name_len);
    mp->host_if_name[name_len] = '\0';  // Ensure null termination

    printf("S\n");

    if (0)
    {
        S(mp);
    }
    else
    {
        ret = vl_socket_client_write();

        if (ret < 0)
        {
            printf("failed ret S = %d\n", ret);
            exit(2);
        }
    }
    printf("bytes setnt ret S = %d\n", ret);

    printf("W\n");

    if (0)
    {
        W(ret);
    }
    else
    {
        f64 timeout = vat_time_now(vam) + 1.0;
        ret = -99;

        printf("read\n");
        // ret = vl_socket_client_read(5);
        ret = vl_socket_client_read_internal(scm, 5);
        printf("read end\n");

        if (ret != 0)
        {
            printf("vl_socket_client_read failed: %d\n", ret);
            exit(2);
        }

        while (vat_time_now (vam) < timeout)
        {
            if (vam->result_ready == 1) {
                ret = vam->retval;
                break;
            }
            vat_suspend(vam->vlib_main, 1e-5);
        }
    }


    printf("SUCCESS\n");

    if (ret != 0)
        fprintf(stderr, "Failed to create host interface\n");
    else
        printf("Host interface '%s' created successfully\n", INTERFACE_NAME);

    vl_socket_client_disconnect();
}

