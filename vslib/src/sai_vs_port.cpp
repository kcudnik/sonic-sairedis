#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ const sai_port_stat_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ const sai_port_stat_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(PORT,port);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)

    vs_get_port_stats,
    vs_clear_port_stats,
    vs_clear_port_all_stats,
};
