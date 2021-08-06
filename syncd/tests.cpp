#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include <sai.h>
}

#include "../lib/inc/Sai.h"
#include "Syncd.h"
#include "MetadataLogger.h"
#include "sairedis.h"
#include "sairediscommon.h"
#include "TimerWatchdog.h"

#include "meta/sai_serialize.h"
#include "meta/OidRefCounter.h"
#include "meta/SaiAttrWrapper.h"
#include "meta/SaiObjectCollection.h"

#include "swss/logger.h"
#include "swss/dbconnector.h"
#include "swss/schema.h"
#include "swss/redisreply.h"
#include "swss/consumertable.h"
#include "swss/select.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <thread>
#include <tuple>

using namespace syncd;

//static bool g_syncMode;

#define ASSERT_SUCCESS(format,...) \
    if ((status)!=SAI_STATUS_SUCCESS) \
        SWSS_LOG_THROW(format ": %s", ##__VA_ARGS__, sai_serialize_status(status).c_str());


int main()
{

    return 0;
}
