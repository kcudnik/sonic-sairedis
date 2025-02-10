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

#define INTERFACE_NAME "vpp1out"
#define VPP_SOCKET_PATH "/run/vpp/api1.sock"

// compile: gcc main.c -o main -lvlib -lvlibapi -lvppapiclient -lvlibmemoryclient -lvppinfra
// vpp v25.02-rc0~169-gf0a126a1e built by kcudnik on ubuntu2 at 2024-12-09T16:08:56

vat_main_t vat_main;

// TODO hack, where is this defined ?
#define __plugin_msg_base 0

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

void scenario1()
{
    // TODO THIS crashes on 
    // clib_mem_heap_alloc_inline (heap=<optimized out>, size=9, align=8, os_out_of_memory_on_failure=1)
    vl_socket_client_connect(VPP_SOCKET_PATH, "myclient", 0);
}

void scenario2()
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

void scenario3()
{
    clib_mem_init_thread_safe(0, 128 << 20);

    int ret = vl_socket_client_connect(VPP_SOCKET_PATH, "myclient", 0);

    if (ret < 0)
    {
        fprintf(stderr, "Failed to connect to VPP socket: %s\n", VPP_SOCKET_PATH);
        fprintf(stderr, "Error Code: %d, Error Message: %s\n", errno, strerror(errno));
        exit(1);
    }

    api_main_t *am = vlibapi_get_main();
    vat_main_t *vam = &vat_main;

    // vpp expects the client index in network order
    vam->my_client_index = htonl (socket_client_main.client_index);
    am->my_client_index = vam->my_client_index;

    // manually create host interface

    vl_api_af_packet_create_t *mp;

    // TODO THIS creashes on:
    // #0  vl_msg_api_alloc_internal (vlib_rp=0x0, nbytes=81, pool=0, may_return_null=0)
    // vlib_rp -> is null, seems like shmem is not initialized
    M(AF_PACKET_CREATE, mp);
    S(mp);
    W(ret);

    if (ret != 0)
        fprintf(stderr, "Failed to create host interface\n");
    else
        printf("Host interface '%s' created successfully\n", INTERFACE_NAME);
}

void scenario4()
{
    clib_mem_init_thread_safe(0, 128 << 20);

    int ret = vl_socket_client_connect(VPP_SOCKET_PATH, "myclient", 0);

    if (ret < 0)
    {
        fprintf(stderr, "Failed to connect to VPP socket: %s\n", VPP_SOCKET_PATH);
        fprintf(stderr, "Error Code: %d, Error Message: %s\n", errno, strerror(errno));
        exit(1);
    }

    api_main_t *am = vlibapi_get_main();
    vat_main_t *vam = &vat_main;

    // vpp expects the client index in network order
    vam->my_client_index = htonl (socket_client_main.client_index);
    am->my_client_index = vam->my_client_index;

    // TODO this creashes on:
    // #0  __GI___pthread_mutex_lock (mutex=0x8) at ../nptl/pthread_mutex_lock.c:67
    // #1  0x00007fdde920a099 in region_lock (rp=0x0, tag=4) at /home/kcudnik/vpp/repo/vpp/src/svm/svm.c:103
    vl_map_shmem("foo", 1);
}
        
int main()
{
    // scenario1();
    // scenario2();
    // scenario3();
    scenario4();

    return 0;
}

