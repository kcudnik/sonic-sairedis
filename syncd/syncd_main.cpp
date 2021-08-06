#include "sairediscommon.h"

#include "CommandLineOptionsParser.h"
#include "Syncd.h"
#include "MetadataLogger.h"
#include "PortMapParser.h"

#include "swss/warm_restart.h"

#ifdef SAITHRIFT
#include <utility>
#include <algorithm>
#include <switch_sai_rpc_server.h>
#endif // SAITHRIFT

#ifdef SAITHRIFT
#define SWITCH_SAI_THRIFT_RPC_SERVER_PORT 9092
#endif // SAITHRIFT

using namespace syncd;

/*
 * Make sure that notification queue pointer is populated before we start
 * thread, and before we create_switch, since at switch_create we can start
 * receiving fdb_notifications which will arrive on different thread and
 * will call getQueueSize() when queue pointer could be null (this=0x0).
 */

int syncd_main(int argc, char **argv)
{

    return EXIT_SUCCESS;
}
