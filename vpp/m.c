#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <vlibmemory/api.h>
#include <vlibapi/api_types.h>
//#include <vpp-api/vpe_msg_enum.h>

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

int main()
{
    return 0;
}
/*
    // Step 1: Set up your socket connection (already done in previous example)
    if (vl_socket_client_connect("/run/vpp/api.sock") < 0) {
        printf("Error: Could not connect to VPP API\n");
        return -1;
    }

    // Step 2: Allocate memory and send request as before
    vl_api_af_packet_create_t *mp;
    mp = vl_msg_api_alloc(sizeof(*mp));
    if (!mp) {
        printf("Error: Message allocation failed\n");
        return -1;
    }
    memset(mp, 0, sizeof(*mp));  // Zero out the structure
    mp->_vl_msg_id = htons(VL_API_AF_PACKET_CREATE);
    strncpy((char *)mp->interface_name, "vpp1out", sizeof(mp->interface_name) - 1);
    mp->hw_address_len = 6;
    memcpy(mp->hw_address, "\x00\x11\x22\x33\x44\x55", 6);

    // Step 3: Send request
    S(mp);  // Send message
    W(ret);  // Wait for the response

    // Step 4: Process the response
    if (ret < 0) {
        printf("Error: AF_PACKET_CREATE request failed\n");
    } else {
        printf("Interface created successfully\n");
        // You can access the response here (e.g., interface index, status)
        vl_api_af_packet_create_reply_t *reply = (vl_api_af_packet_create_reply_t *)ret;
        if (reply->retval == 0) {
            printf("AF_PACKET interface created with index: %d\n", reply->interface_index);
        } else {
            printf("Error: %d\n", reply->retval);
        }
    }

    // Step 5: Disconnect when done
    vl_socket_client_disconnect();

    return 0;
}
}
*/

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <unistd.h>
//#include <vlibapi/api.h>
//#include <vlibmemory/api.h>
////#include <vppinfra/cli.h>
//#include <vppinfra/error.h>
////#include <vppinfra/svm.h>
//
//#define SHMEM_SIZE (64 * 1024 * 1024)  // Define shared memory size (64 MB)
//#define SHM_NAME "/run/vpp/api-shm"  // Shared memory name for VPP
//
//int main2() {
//    // Step 1: Initialize shared memory and connect to VPP
//
//    // Initialize shared memory for VPP API
//    svm_region_t *vlib_rp = NULL;
//    vl_api_shm_elem_config_t config = {0};
//    config.num_elements = 1024;
//    config.elem_size = 128;  // Example size of each element
//
//    // Map the shared memory region
//    vlib_rp = vl_map_shmem(SHM_NAME, 1, SHMEM_SIZE);  // 1 means VPP application
//    if (!vlib_rp) {
//        printf("Error: Failed to map shared memory region\n");
//        return -1;
//    }
//
//    printf("Shared memory mapped successfully\n");
//
//    // Step 2: Connect to VPP API via the socket
//    if (vl_socket_client_connect("/run/vpp/api.sock") < 0) {
//        printf("Error: Could not connect to VPP API\n");
//        return -1;
//    }
//
//    // Step 3: Send AF_PACKET_CREATE message to VPP
//
//    // Allocate and fill the message structure
//    vl_api_af_packet_create_t *mp;
//    mp = vl_msg_api_alloc(sizeof(*mp));
//    if (!mp) {
//        printf("Error: Failed to allocate message\n");
//        return -1;
//    }
//
//    memset(mp, 0, sizeof(*mp));  // Zero out the structure
//
//    // Fill in the necessary fields for AF_PACKET_CREATE
//    mp->_vl_msg_id = htons(VL_API_AF_PACKET_CREATE);
//    strncpy((char *)mp->interface_name, "vpp1out", sizeof(mp->interface_name) - 1);  // Interface name
//    mp->hw_address_len = 6;  // Length of the hardware address (MAC)
//    memcpy(mp->hw_address, "\x00\x11\x22\x33\x44\x55", 6);  // Example MAC address
//
//    // Step 4: Send the message and wait for the response
//    S(mp);  // Send message
//    W(ret);  // Wait for the response
//
//    // Step 5: Process the response
//    if (ret < 0) {
//        printf("Error: AF_PACKET_CREATE request failed\n");
//    } else {
//        printf("Interface created successfully\n");
//        vl_api_af_packet_create_reply_t *reply = (vl_api_af_packet_create_reply_t *)ret;
//        if (reply->retval == 0) {
//            printf("AF_PACKET interface created with index: %d\n", reply->interface_index);
//        } else {
//            printf("Error: %d\n", reply->retval);
//        }
//    }
//
//    // Step 6: Disconnect from VPP API
//    vl_socket_client_disconnect();
//
//    return 0;
//}
//
