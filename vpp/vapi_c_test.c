//gcc vapi.c vapi_c_test.c \
//-lvlib -lvlibapi -lvppapiclient -lvlibmemoryclient -lvppinfra -lpthread -lsvm -lcheck -lm -lrt -lsubunit

#include <stdio.h>
#include <check.h> // tsting

#include <vapi/interface.api.vapi.h>
#include <vapi/ip.api.vapi.h>
#include <vapi/af_packet.api.vapi.h>
#include <vapi/ipip.api.vapi.h>


//DEFINE_VAPI_MSG_IDS_VPE_API_JSON;
DEFINE_VAPI_MSG_IDS_INTERFACE_API_JSON; // if flags
DEFINE_VAPI_MSG_IDS_IP_API_JSON;
//DEFINE_VAPI_MSG_IDS_L2_API_JSON;
DEFINE_VAPI_MSG_IDS_AF_PACKET_API_JSON; // needed for af_packet
DEFINE_VAPI_MSG_IDS_IPIP_API_JSON; // needed for ipip

static char *app_name = NULL;
static char *api_prefix = NULL;
static bool use_uds = true;
static const int max_outstanding_requests = 64;
static const int response_queue_size = 32;

#ifndef ck_assert_ptr_eq
#define ck_assert_ptr_eq(X,Y) ck_assert_int_eq((long)X, (long)Y)
#endif
#ifndef ck_assert_ptr_ne
#define ck_assert_ptr_ne(X,Y) ck_assert_int_ne((long)X, (long)Y)
#endif

static vapi_ctx_t ctx;

void
setup_blocking (void)
{
  vapi_error_e rv = vapi_ctx_alloc (&ctx);
  ck_assert_int_eq (VAPI_OK, rv);
  rv =
    vapi_connect_ex (ctx, app_name, api_prefix, max_outstanding_requests,
		     response_queue_size, VAPI_MODE_BLOCKING, true, use_uds);
  ck_assert_int_eq (VAPI_OK, rv);
}

void
setup_nonblocking (void)
{
  vapi_error_e rv = vapi_ctx_alloc (&ctx);
  ck_assert_int_eq (VAPI_OK, rv);
  rv = vapi_connect_ex (ctx, app_name, api_prefix, max_outstanding_requests,
			response_queue_size, VAPI_MODE_NONBLOCKING, true,
			use_uds);
  ck_assert_int_eq (VAPI_OK, rv);
}

void
teardown (void)
{
  vapi_disconnect (ctx);
  vapi_ctx_free (ctx);
}

vapi_type_interface_index
create_host_name(
        const char* ifname)
{
      vapi_msg_af_packet_create_v3 *pc = vapi_alloc_af_packet_create_v3(ctx);
      ck_assert_ptr_ne (NULL, pc);

      // auto populated
      // DBG printf("msg id: %d, context: %d\n", pc->header._vl_msg_id, pc->header.context); // dbg

      int len = strlen(ifname);

      pc->payload.use_random_hw_addr = 1;
      strncpy((char *)pc->payload.host_if_name, ifname, len); 
      pc->payload.host_if_name[len] = 0;

      pc->payload.mode = AF_PACKET_API_MODE_ETHERNET;
      pc->payload.rx_frame_size = 2048;
      pc->payload.tx_frame_size = 2048; //67584;
      pc->payload.tx_frame_size = 2048*2; // 67584;
      pc->payload.rx_frames_per_block = 32;
      pc->payload.tx_frames_per_block = 1024;
      pc->payload.flags = AF_PACKET_API_FLAG_QDISC_BYPASS | AF_PACKET_API_FLAG_CKSUM_GSO;
      pc->payload.num_rx_queues = 1;
      pc->payload.num_tx_queues = 1;

      // printf("host: %s\n", pc->payload.host_if_name); // dbg

      vapi_msg_af_packet_create_v3_hton(pc); // TODO is this needed ?

      //printf("send\n");
      vapi_error_e rv = vapi_send(ctx, pc);
      //printf("send rv: %d\n", rv);

      ck_assert_int_eq(VAPI_OK, rv);

      vapi_msg_af_packet_create_v3_reply *resp;

      size_t size;
      rv = vapi_recv(ctx, (void *) &resp, &size, 0, 0);
      //printf("recv: %d\n", rv);

      ck_assert_int_eq (VAPI_OK, rv);

      vapi_msg_af_packet_create_v3_reply_ntoh(resp); // check for  OK

      //int placeholder;
      //af_packet_create_cb(NULL, &placeholder, VAPI_OK, true, &resp->payload);

      //ck_assert_int_eq (VAPI_OK, rv);
      //ck_assert_int_eq (true, is_last);
      
      vapi_payload_af_packet_create_v3_reply *p = &resp->payload;

      printf("retval: %d, sw_if_index: %d\n", p->retval, p->sw_if_index);

      int sw_if_index = p->sw_if_index;

      if (p->retval != 0)
          printf("ERROR: create hostif failed\n");
      else
          printf("create interface SUCCESS\n");

      vapi_msg_free(ctx, resp); // ctx destroyed, can't use payload any more !

      return sw_if_index;
}

void sw_interface_set_flags(
        vapi_type_interface_index sw_if_index,
        vapi_enum_if_status_flags flags)
{
    //      vapi_alloc_sw_interface_set_flags

    vapi_msg_sw_interface_set_flags *isf =  vapi_alloc_sw_interface_set_flags(ctx); // alloc
    ck_assert_ptr_ne (NULL, isf);

    isf->payload.sw_if_index = sw_if_index; // populate payload
    isf->payload.flags = flags;

    printf("sw_idx: %d, flags: %d\n", sw_if_index, flags);

    vapi_msg_sw_interface_set_flags_hton(isf); // msg endian

    vapi_error_e rv = vapi_send(ctx, isf);
    ck_assert_int_eq(VAPI_OK, rv);

    vapi_msg_sw_interface_set_flags_reply *resp;

    size_t size;
    rv = vapi_recv(ctx, (void *) &resp, &size, 0, 0);
    printf("recv: %d\n", rv);
    ck_assert_int_eq (VAPI_OK, rv);

    vapi_msg_sw_interface_set_flags_reply_ntoh(resp); // reply endian

    // payload contains only retval
    vapi_payload_sw_interface_set_flags_reply *p = &resp->payload;

    printf("retval: %d\n", p->retval);

    if (p->retval != 0)
        printf("ERROR: SW interface state flags\n");
    else
        printf("SW set interface state flags SUCCESS\n");

    vapi_msg_free(ctx, resp);
}

void add_del_ip_address(
        vapi_type_interface_index sw_if_index,
        vapi_type_address_with_prefix *prefix,
        bool is_del)
{
    vapi_msg_sw_interface_add_del_address *iada = vapi_alloc_sw_interface_add_del_address(ctx); // alloc
    ck_assert_ptr_ne (NULL, iada);

    iada->payload.sw_if_index = sw_if_index; // populate payload
    iada->payload.is_add = !is_del;
    iada->payload.del_all = false;
    iada->payload.prefix = *prefix;

    // vapi_type_address_with_prefix prefix;
    //

    printf("sw_idx: %d\n", sw_if_index);

    vapi_msg_sw_interface_add_del_address_hton(iada); // msg endian

    vapi_error_e rv = vapi_send(ctx, iada);
    ck_assert_int_eq(VAPI_OK, rv);

    vapi_msg_sw_interface_add_del_address_reply *resp;

    size_t size;
    rv = vapi_recv(ctx, (void *) &resp, &size, 0, 0);
    printf("recv: %d\n", rv);
    ck_assert_int_eq (VAPI_OK, rv);

    vapi_msg_sw_interface_add_del_address_reply_ntoh(resp); // reply endian

    // payload contains only retval
    vapi_payload_sw_interface_add_del_address_reply *p = &resp->payload;

    printf("retval: %d\n", p->retval);

    if (p->retval != 0)
        printf("ERROR: SW interface add del address\n");
    else
        printf("SW set interface add del address SUCCESS\n");

    vapi_msg_free(ctx, resp);
}

int create_ipip_tunnel(
        vapi_type_ipip_tunnel *tunnel)
{
    vapi_msg_ipip_add_tunnel *tun = vapi_alloc_ipip_add_tunnel(ctx);
    ck_assert_ptr_ne (NULL, tun);

    tun->payload.tunnel = *tunnel; // populate payload

    vapi_msg_ipip_add_tunnel_hton(tun);

    vapi_error_e rv = vapi_send(ctx, tun);
    ck_assert_int_eq(VAPI_OK, rv);

    vapi_msg_ipip_add_tunnel_reply *resp;

    size_t size;
    rv = vapi_recv(ctx, (void *) &resp, &size, 0, 0);
    ck_assert_int_eq (VAPI_OK, rv);

    vapi_msg_ipip_add_tunnel_reply_ntoh(resp);

    vapi_payload_ipip_add_tunnel_reply *p = &resp->payload;

    printf("retval: %d, sw_if_index: %d\n", p->retval, p->sw_if_index);

    int sw_if_index = p->sw_if_index;

    if (p->retval != 0)
        printf("ERROR: add ipip tunnel failed\n");
    else
        printf("add ipip tunnel SUCCESS\n");

    vapi_msg_free(ctx, resp); // ctx destroyed, can't use payload any more !

    return sw_if_index;
}

void route_add_del(
        vapi_type_prefix* prefix,
        int sw_if_index)
{
    printf("creating route!\n");
    // NOTE: there is v2 version

    vapi_msg_ip_route_add_del_v2 *req = vapi_alloc_ip_route_add_del_v2(ctx,1); // 1 path
    ck_assert_ptr_ne (NULL, req);

    // Set payload
    req->payload.is_add = 1; // 1 = add route, 0 = delete
    req->payload.is_multipath = 0; // Single path
    req->payload.route.table_id = 0; // TODO default ?
    req->payload.route.stats_index = 0; // TODO
    req->payload.route.prefix = *prefix;
    req->payload.route.n_paths = 1;
    req->payload.route.src = 0; // TODO ?
    req->payload.route.paths[0].sw_if_index = sw_if_index; // Use ipip0 interface
    req->payload.route.paths[0].proto = FIB_API_PATH_NH_PROTO_IP4;
    req->payload.route.paths[0].weight = 1; // Default weight
    req->payload.route.paths[0].table_id = 0; // default
    req->payload.route.paths[0].n_labels = 0;

    vapi_msg_ip_route_add_del_v2_hton(req);

    vapi_error_e rv = vapi_send(ctx, req);
    ck_assert_int_eq(VAPI_OK, rv);

    vapi_msg_ip_route_add_del_v2_reply *resp;

    size_t size;
    rv = vapi_recv(ctx, (void *) &resp, &size, 0, 0);
    ck_assert_int_eq (VAPI_OK, rv);

    vapi_msg_ip_route_add_del_v2_reply_ntoh(resp);

    vapi_payload_ip_route_add_del_v2_reply *p = &resp->payload;

    if (p->retval != 0)
        printf("ERROR: add ip route failed\n");
    else
        printf("add ip route SUCCESS: stats_index: %d\n", p->stats_index);

}

START_TEST (test_cfg_vpp1)
{
  printf ("--- XXX configure VPP1 ---\n");

  // TODO we need sw_if_index for each one to operate

  // TODO should be done other way, to get api exit code
  int idxA = create_host_name("vpp1out"); // create host name vpp1out
  int idxB = create_host_name("vpp1vpp2");

  // TODO if using hostname we need to list interfaces

  sw_interface_set_flags(idxA, IF_STATUS_API_FLAG_ADMIN_UP | IF_STATUS_API_FLAG_LINK_UP); // set int state host-vpp1out up
  sw_interface_set_flags(idxB, IF_STATUS_API_FLAG_ADMIN_UP | IF_STATUS_API_FLAG_LINK_UP);

  vapi_type_address_with_prefix p1;
  vapi_type_address_with_prefix p2;

  // TODO make helper methods to populate this
  p1.len = 24;
  p1.address.af = ADDRESS_IP4;
  p1.address.un.ip4[0] = 10;
  p1.address.un.ip4[1] = 0;
  p1.address.un.ip4[2] = 0;
  p1.address.un.ip4[3] = 2;

  p2.len = 24;
  p2.address.af = ADDRESS_IP4;
  p2.address.un.ip4[0] = 10;
  p2.address.un.ip4[1] = 0;
  p2.address.un.ip4[2] = 3;
  p2.address.un.ip4[3] = 1;

  add_del_ip_address(idxA, &p1, false); // set int ip addr host-vpp1out 10.0.0.2/24
  add_del_ip_address(idxB, &p2, false);

  // tunnel !

  vapi_type_ipip_tunnel tunnel;

  u32 fib_index = 0;
  u32 table_id = 0; // outer-table-id (by default 0)

  bool ip6_set = false;

  fib_index = 0; // fib_table_find(fib_ip_proto (ip6_set), table_id);

  printf ("fib_index = %d, table_id = %d\n", fib_index, table_id);

  tunnel.instance = ~0; // auto assigned
  tunnel.src.af = ADDRESS_IP4; // TODO add api to populate this
  tunnel.src.un.ip4[0] = 10; // ip4 = (vapi_type_ip4_address){192, 168, 2, 1};
  tunnel.src.un.ip4[1] = 0;
  tunnel.src.un.ip4[2] = 3;
  tunnel.src.un.ip4[3] = 1;
  tunnel.dst.af = ADDRESS_IP4;
  tunnel.dst.un.ip4[0] = 10;
  tunnel.dst.un.ip4[1] = 0;
  tunnel.dst.un.ip4[2] = 3;
  tunnel.dst.un.ip4[3] = 2;
  tunnel.sw_if_index = ~0; // TODO which one? hwo to obtain this?
  tunnel.table_id = fib_index; // TODO table id or fib id ?
  tunnel.flags = TUNNEL_API_ENCAP_DECAP_FLAG_NONE;
  tunnel.mode = TUNNEL_API_MODE_P2P;
  tunnel.dscp = IP_API_DSCP_CS0;
    
  if (fib_index == ~0)
  {
      printf("ERROR: no such fib\n");
      return;
      // TODO sw_if_index to invalid
  }

  int idxC = create_ipip_tunnel(&tunnel);

  printf("ipip sw index: %d\n", idxC);

  // TODO check idxC if success

  sw_interface_set_flags(idxC, IF_STATUS_API_FLAG_ADMIN_UP | IF_STATUS_API_FLAG_LINK_UP);

  vapi_type_address_with_prefix p3;

  p3.len = 32;
  p3.address.af = ADDRESS_IP4; // dummy ip address
  p3.address.un.ip4[0] = 1;
  p3.address.un.ip4[1] = 1;
  p3.address.un.ip4[2] = 1;
  p3.address.un.ip4[3] = 1;

  add_del_ip_address(idxC, &p3, false); // set int ip addr host-vpp1out 10.0.0.2/24

  // ADD ROUTE
  //
  vapi_type_prefix p4;

  p4.address.af = ADDRESS_IP4;
  p4.address.un.ip4[0] = 10;
  p4.address.un.ip4[1] = 0;
  p4.address.un.ip4[2] = 1;
  p4.address.un.ip4[3] = 0;
  p4.len = 24; // CIDR notation

  route_add_del(&p4, idxC);

//DONE $VPP1 create ipip tunnel src 10.0.3.1 dst 10.0.3.2
//DONE $VPP1 set int state ipip0 up
//DONE $VPP1 set int ip addr ipip0 1.1.1.1/32
//DONE $VPP1 ip route add 10.0.1.0/24 via ipip0

}
END_TEST;

START_TEST (test_cfg_vpp2)
{
  printf ("--- XXX configure VPP2 ---\n");

  // TODO we need sw_if_index for each one to operate

  // TODO should be done other way, to get api exit code
  int idxA = create_host_name("vpp2out"); // create host name vpp2out
  int idxB = create_host_name("vpp2vpp1");

  // TODO if using hostname we need to list interfaces

  sw_interface_set_flags(idxA, IF_STATUS_API_FLAG_ADMIN_UP | IF_STATUS_API_FLAG_LINK_UP); // set int state host-vpp2out up
  sw_interface_set_flags(idxB, IF_STATUS_API_FLAG_ADMIN_UP | IF_STATUS_API_FLAG_LINK_UP);

  vapi_type_address_with_prefix p1;
  vapi_type_address_with_prefix p2;

  // TODO make helper methods to populate this
  p1.len = 24;
  p1.address.af = ADDRESS_IP4;
  p1.address.un.ip4[0] = 10;
  p1.address.un.ip4[1] = 0;
  p1.address.un.ip4[2] = 1;
  p1.address.un.ip4[3] = 2;

  p2.len = 24;
  p2.address.af = ADDRESS_IP4;
  p2.address.un.ip4[0] = 10;
  p2.address.un.ip4[1] = 0;
  p2.address.un.ip4[2] = 3;
  p2.address.un.ip4[3] = 2;

  add_del_ip_address(idxA, &p1, false); // set int ip addr host-vpp2out 10.0.1.2/24
  add_del_ip_address(idxB, &p2, false);
  // set int ip addr

  vapi_type_ipip_tunnel tunnel;

  u32 fib_index = 0;
  u32 table_id = 0; // outer-table-id (by default 0)

  bool ip6_set = false;

  fib_index = 0; // fib_table_find(fib_ip_proto (ip6_set), table_id);

  printf ("fib_index = %d, table_id = %d\n", fib_index, table_id);

  tunnel.instance = ~0; // auto assigned
  tunnel.src.af = ADDRESS_IP4; // TODO add api to populate this
  tunnel.src.un.ip4[0] = 10; // ip4 = (vapi_type_ip4_address){192, 168, 2, 1};
  tunnel.src.un.ip4[1] = 0;
  tunnel.src.un.ip4[2] = 3;
  tunnel.src.un.ip4[3] = 2;
  tunnel.dst.af = ADDRESS_IP4;
  tunnel.dst.un.ip4[0] = 10;
  tunnel.dst.un.ip4[1] = 0;
  tunnel.dst.un.ip4[2] = 3;
  tunnel.dst.un.ip4[3] = 1;
  tunnel.sw_if_index = ~0; // TODO which one? hwo to obtain this?
  tunnel.table_id = fib_index; // TODO table id or fib id ?
  tunnel.flags = TUNNEL_API_ENCAP_DECAP_FLAG_NONE;
  tunnel.mode = TUNNEL_API_MODE_P2P;
  tunnel.dscp = IP_API_DSCP_CS0;
    
  if (fib_index == ~0)
  {
      printf("ERROR: no such fib\n");
      return;
      // TODO sw_if_index to invalid
  }

  int idxC = create_ipip_tunnel(&tunnel);

  printf("ipip sw index: %d\n", idxC);

  // TODO check idxC if success

  sw_interface_set_flags(idxC, IF_STATUS_API_FLAG_ADMIN_UP | IF_STATUS_API_FLAG_LINK_UP);

  vapi_type_address_with_prefix p3;

  p3.len = 32;
  p3.address.af = ADDRESS_IP4; // dummy ip address
  p3.address.un.ip4[0] = 1;
  p3.address.un.ip4[1] = 1;
  p3.address.un.ip4[2] = 1;
  p3.address.un.ip4[3] = 1;

  add_del_ip_address(idxC, &p3, false); // set int ip addr host-vpp1out 10.0.0.2/24

  // ADD ROUTE
  vapi_type_prefix p4;

  p4.address.af = ADDRESS_IP4;
  p4.address.un.ip4[0] = 10;
  p4.address.un.ip4[1] = 0;
  p4.address.un.ip4[2] = 0;
  p4.address.un.ip4[3] = 0;
  p4.len = 24; // CIDR notation

  route_add_del(&p4, idxC);

//DONE $VPP2 create ipip tunnel src 10.0.3.2 dst 10.0.3.1
//DONE $VPP2 set int state ipip0 up
//DONE $VPP2 set int ip addr ipip0 1.1.1.1/32
//DONE $VPP2 ip route add 10.0.0.0/24 via ipip0

}
END_TEST;

void setup_blocking1()
{
  api_prefix = "/run/vpp/api1.sock";
  setup_blocking();
}

void setup_blocking2()
{
  api_prefix = "/run/vpp/api2.sock";
  setup_blocking();
}

Suite *
test_suite (void)
{
  Suite *s = suite_create ("VAPI test");

  TCase *tc_block1 = tcase_create ("Configure VPP1");
  tcase_set_timeout (tc_block1, 25);
  tcase_add_checked_fixture (tc_block1, setup_blocking1, teardown);
  suite_add_tcase (s, tc_block1);
  tcase_add_test (tc_block1, test_cfg_vpp1);
  
  TCase *tc_block2 = tcase_create ("Configure VPP2");
  tcase_set_timeout (tc_block2, 25);
  tcase_add_checked_fixture (tc_block2, setup_blocking2, teardown);
  suite_add_tcase (s, tc_block2);
  tcase_add_test (tc_block2, test_cfg_vpp2);

  return s;
}

int main (int argc, char *argv[])
{
    if (3 != argc)
    {
        printf ("Invalid argc==`%d'\n", argc);
        return EXIT_FAILURE;
    }
    app_name = argv[1];
    api_prefix = argv[2];

    printf ("App name: `%s', API prefix: `%s'\n", app_name, api_prefix);

    int number_failed;
    Suite *s;
    SRunner *sr;

    s = test_suite ();
    sr = srunner_create (s);

    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
