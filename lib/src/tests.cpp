extern "C" {
#include "saimetadata.h"
}

#include "ContextConfigContainer.h"

#include "swss/logger.h"
#include "swss/table.h"
#include "swss/tokenize.h"

#include "lib/inc/Recorder.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include <unistd.h>

#include <iostream>
#include <chrono>
#include <vector>

#define ASSERT_EQ(a,b) if ((a) != (b)) { SWSS_LOG_THROW("ASSERT EQ FAILED: " #a " != " #b); }


int main()
{
    SWSS_LOG_ENTER();

    std::cout << " * test tokenize bulk route entry" << std::endl;


    return 0;
}
