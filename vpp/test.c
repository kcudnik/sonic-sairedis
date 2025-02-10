
//#include <vpp-api/vat/vat.h>
//#include <vpp-api/vpe/api.h>
#include <vlibapi/api.h>
#include <vlibmemory/api.h>

#include <vat/vat.h>
#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include <vppinfra/error.h>

#include <vpp_plugins/af_packet/af_packet.h>
//#include <vpp_plugins/af_packet/af_packet.api.h>
#include <vpp_plugins/af_packet/af_packet.api_types.h>
#include <vpp_plugins/af_packet/af_packet.api_enum.h>

#include <vlibapi/vat_helper_macros.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define INTERFACE_NAME "vpp1out"
//#define VPP_SOCKET_PATH "/run/vpp/cli-vpp1.sock"
#define VPP_SOCKET_PATH "/run/vpp/api1.sock"

vat_main_t vat_main;

#define __plugin_msg_base 0 // hack, where is this defined ?

void create_host_interface(const char*ifname)
{
    vat_main_t *vam = &vat_main;
//
//    vl_api_create_host_interface_t *mp;
//    
//    u32 name_len = strlen(ifname);
//
//    // Allocate API message
//    M(CREATE_HOST_INTERFACE, create_host_interface);
//
//    // Copy interface name into API message
//    strncpy((char *)mp->host_if_name, ifname, name_len);
//    mp->host_if_name[name_len] = '\0';  // Ensure null termination
//
//    // Send API request
//    S(mp);
//
//    // Wait for reply
//    W(ret);
//
//    if (ret != 0)
//    {
//        fprintf(stderr, "Failed to create host interface\n");
//    }
//    else
//    {
//        printf("Host interface '%s' created successfully\n", ifname);
//    }

    // explicit hostif create

    //af_packet_create_if_arg_t arg;
    //int rv = af_packet_create_if(&arg);

    api_main_t *am = vlibapi_get_main();

    printf("*am: 0x%p\n", am);

    printf("am->vlib_rp: 0x%p\n", am->vlib_rp);

    int ret;

    vl_api_af_packet_create_t *mp;

    printf("* prepare M\n");
    //M(AF_PACKET_CREATE, mp);

    mp = vl_msg_api_alloc(sizeof(*mp));
    if (!mp) {
        printf("Error: Failed to allocate message\n");
        exit(3);
    }

    memset(mp, 0, sizeof(*mp));  // Zero out the structure

    // Fill in the necessary fields for AF_PACKET_CREATE
    mp->_vl_msg_id = htons(VL_API_AF_PACKET_CREATE);
    //strncpy((char *)mp->interface_name, "vpp1out", sizeof(mp->interface_name) - 1);  // Interface name
    //mp->hw_address_len = 6;  // Length of the hardware address (MAC)
    //memcpy(mp->hw_address, "\x00\x11\x22\x33\x44\x55", 6);  // Example MAC address


    printf("* prepared!\n");

    // TODO validate inputs

    u32 name_len = strlen(ifname);

    // Copy interface name into API message
    strncpy((char *)mp->host_if_name, ifname, name_len);
    mp->host_if_name[name_len] = '\0';  // Ensure null termination

    printf("* send S\n");

    S(mp);

    printf("* waint W\n");

    W(ret);

    if (ret != 0)
    {
        fprintf(stderr, "Failed to create host interface\n");
        exit(1);
    }
    else
    {
        printf("Host interface '%s' created successfully\n", ifname);
    }

}

uword *interface_name_by_sw_index = NULL;

int vsc_socket_connect(vat_main_t *vam, char *client_name)
{
    int rv;

    api_main_t *am = vlibapi_get_main();

    vam->socket_client_main = &socket_client_main;

    printf("SOCKET: %s\n",vam->socket_name);

    if ((rv = vl_socket_client_connect(VPP_SOCKET_PATH,
                    //(char *) vam->socket_name,
                    client_name,
                    0 /* default socket rx, tx buffer */ )))
        return rv;

    /* vpp expects the client index in network order */

    vam->my_client_index = htonl (socket_client_main.client_index);

    am->my_client_index = vam->my_client_index;

    return 0;
}

f64
vat_time_now (vat_main_t * vam)
{
#if VPP_API_TEST_BUILTIN
  return vlib_time_now (vam->vlib_main);
#else
  return clib_time_now (&vam->clib_time);
#endif
}

void __clib_no_tail_calls
vat_suspend (vlib_main_t *vm, f64 interval)
{
    const struct timespec req = {0, 100000000};
    nanosleep(&req, NULL);
}

static int
api_sock_init_shm (vat_main_t * vam)
{
#if VPP_API_TEST_BUILTIN == 0
    printf("1\n");
    unformat_input_t *i = vam->input;
    vl_api_shm_elem_config_t *config = 0;
    u64 size = 64 << 20;
    int rv;

    printf("2: 0x%p\n", i);

    while (unformat_check_input (i) != UNFORMAT_END_OF_INPUT)
    {
    printf("3\n");
        if (unformat (i, "size %U", unformat_memory_size, &size))
            ;
        else
            break;
    }
    printf("4\n");

    /*
     *    * Canned custom ring allocator config.
     *       * Should probably parse all of this
     *          */
    vec_validate (config, 6);
    config[0].type = VL_API_VLIB_RING;
    config[0].size = 256;
    config[0].count = 32;

    config[1].type = VL_API_VLIB_RING;
    config[1].size = 1024;
    config[1].count = 16;

    config[2].type = VL_API_VLIB_RING;
    config[2].size = 4096;
    config[2].count = 2;

    config[3].type = VL_API_CLIENT_RING;
    config[3].size = 256;
    config[3].count = 32;

    config[4].type = VL_API_CLIENT_RING;
    config[4].size = 1024;
    config[4].count = 16;

    config[5].type = VL_API_CLIENT_RING;
    config[5].size = 4096;
    config[5].count = 2;

    config[6].type = VL_API_QUEUE;
    config[6].count = 128;
    config[6].size = sizeof (uword);

    printf("1\n");
    rv = vl_socket_client_init_shm (config, 1 /* want_pthread */ );
    if (!rv)
        vam->client_index_invalid = 1;
    return rv;
#else
    return -99;
#endif
}

/*
void
vl_noop_handler (void *mp)
{
}

static void
vl_msg_api_set_handlers (int id, char *name, void *handler, void *cleanup,
                         void *endian, int size, int traced,
                         void *tojson, void *fromjson, void *calc_size)
{
    vl_msg_api_msg_config_t cfg;
    vl_msg_api_msg_config_t *c = &cfg;

    clib_memset (c, 0, sizeof (*c));

    c->id = id;
    c->name = name;
    c->handler = handler;
    c->cleanup = cleanup;
    c->endian = endian;
    c->traced = traced;
    c->replay = 1;
    c->message_bounce = 0;
    c->is_mp_safe = 0;
    c->is_autoendian = 0;
    c->tojson = tojson;
    c->fromjson = fromjson;
    c->calc_size = calc_size;
    vl_msg_api_config (c);
}

#define vl_api_get_first_msg_id_reply_t_handler vl_noop_handler
#define vl_api_get_first_msg_id_reply_t_handler_json vl_noop_handler

#define MEMCLNT_MSG_ID(id)  VL_API_##id

#define foreach_vpe_base_api_reply_msg                   \
    _(MEMCLNT_MSG_ID(GET_FIRST_MSG_ID_REPLY), get_first_msg_id_reply) \
    _(MEMCLNT_MSG_ID(CONTROL_PING_REPLY), control_ping_reply)

static void vpp_base_vpe_init(void)
{
#define _(N,n)                                                  \
    vl_msg_api_set_handlers(N+1,                                \
                            #n,                                 \
                            vl_api_##n##_t_handler,             \
                            vl_noop_handler,                    \
                            vl_api_##n##_t_endian,              \
                            sizeof(vl_api_##n##_t), 1,          \
                            vl_api_##n##_t_tojson,              \
                            vl_api_##n##_t_fromjson,            \
                            vl_api_##n##_t_calc_size);

    foreach_vpe_base_api_reply_msg;
 #undef _
 }
*/

#define SHMEM_SIZE (64 * 1024 * 1024)  // 64 MB of shared memory size

        
int main()
{
    vat_main_t *vam = &vat_main;

    // vpp_mutex_lock_init();

    clib_mem_init_thread_safe(0, 128 << 20);
    vlib_main_init();
    clib_time_init (&vam->clib_time);

    /* Set up the plugin message ID allocator right now... */
    vl_msg_api_set_first_available_msg_id (VL_MSG_MEMCLNT_LAST + 1);

    api_main_t *am = vlibapi_get_main();

   // // Step 1: Initialize shared memory configuration
   // vl_api_shm_elem_config_t config = {0};

   // // Step 2: Initialize the shared memory region using vl_init_shmem
   // svm_region_t *vlib_rp = NULL;  // Shared memory region pointer

   // if (vl_init_shmem(vlib_rp, &config, 1, 0) < 0) {  // 1 for VPP app, 0 for shared region
   //     printf("Error: Shared memory initialization failed\n");
   //     return -1;
   // }


    //vpp_base_vpe_init();
    //vam->socket_name = format (0, "%s%c", API_SOCKET_FILE, 0);
    //vam->sw_if_index_by_interface_name = hash_create_string (0, sizeof (uword));
    //interface_name_by_sw_index = hash_create (0, sizeof (uword));

    //if (vsc_socket_connect(vam, "test_vpp_api_client") != 0)
    //{
    //    exit(2);
    //    fprintf(stderr, "Failed to connect to VPP socket: %s\n", VPP_SOCKET_PATH);
    //    fprintf(stderr, "Error Code: %d, Error Message: %s\n", errno, strerror(errno));
    //    exit(1);

    //}

    //printf("SUCCESS CONNECT to socket!\n");

    //exit(3);

    // Connect to VPP via socket
    int ret = vl_socket_client_connect(VPP_SOCKET_PATH, "my client", 0);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to connect to VPP socket: %s (ret: %d)\n", VPP_SOCKET_PATH, ret);
        fprintf(stderr, "Error Code: %d, Error Message: %s\n", errno, strerror(errno));
        exit(1);
    }

    //printf("x\n");
    //if (vl_map_shmem("foo", 1) < 0)
    //{
    //    printf("sh mem failed!\n");
    //    exit(1);
    //}
    //printf("1\n");

    //printf("sh mem success!\n");

      /* vpp expects the client index in network order */
    vam->my_client_index = htonl (socket_client_main.client_index);
    am->my_client_index = vam->my_client_index;

    printf("Connected to VPP via socket: %s\n", VPP_SOCKET_PATH);

    //vl_api_memclnt_create_t *mp;
    //M(MEMCLNT_CREATE, mp);
    //S(mp);
    //W(ret);

    //// Step 2: Initialize shared memory for API messages
    //vl_client_api_init();

    //api_sock_init_shm(vam);

    //// Step 3: Verify shared memory
    //void *shmem_base = vl_msg_api_get_shmem();
    //if (!shmem_base) {
    //    printf("Error: Shared memory not initialized!\n");
    //    return -1;
    //}

    // VPP1: create host name vpp1out

#define SHMEM_SIZE (64 * 1024 * 1024)  // Define shared memory size (64 MB)
#define SHM_NAME "/run/vpp/api-shm"  // Shared memory name for VPP

        // Step 1: Initialize shared memory and connect to VPP

        // Initialize shared memory for VPP API
        svm_region_t *vlib_rp = NULL;
        vl_api_shm_elem_config_t config = {0};
        config.count = 1024;
        config.size = 128;  // Example size of each element

        // Map the shared memory region
        vl_map_shmem(SHM_NAME, 1);  // 1 means VPP application



    // Create the host interface
    create_host_interface(INTERFACE_NAME);

    printf("disconnecting from VPP\n");

    // Disconnect from VPP API
    vl_client_disconnect(); // TODO: causes core dump sigsev

    // Program terminated with signal SIGSEGV, Segmentation fault.
    // #0  0x00007f55143a514f in vl_msg_api_alloc (nbytes=15) at /home/kcudnik/vpp/repo/vpp/src/vlibapi/memory_shared.c:210
    // 210       pool = (am->our_pid == shmem_hdr->vl_pid);
    // (gdb) bt
    // #0  0x00007f55143a514f in vl_msg_api_alloc (nbytes=15) at /home/kcudnik/vpp/repo/vpp/src/vlibapi/memory_shared.c:210
    // #1  0x00007f551414d9c3 in vl_client_send_disconnect (do_cleanup=0 '\000') at /home/kcudnik/vpp/repo/vpp/src/vlibmemory/memory_client.c:272
    // #2  vl_client_disconnect () at /home/kcudnik/vpp/repo/vpp/src/vlibmemory/memory_client.c:292
    // #3  0x00005611b62d0a9c in main ()


    printf("Disconnect from VPP");

    return 0;
}
