#include "SaiSwitchInterface.h"

#include "swss/logger.h"

using namespace syncd;

SaiSwitchInterface::SaiSwitchInterface(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t switchRid):
    m_switch_vid(switchVid),
    m_switch_rid(switchRid)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_object_id_t SaiSwitchInterface::getVid() const
{
    SWSS_LOG_ENTER();

    return m_switch_vid;
}

sai_object_id_t SaiSwitchInterface::getRid() const
{
    SWSS_LOG_ENTER();

    return m_switch_rid;
}

