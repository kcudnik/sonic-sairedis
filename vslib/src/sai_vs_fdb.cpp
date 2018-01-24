#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t internal_vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * There are 2 databases for fdb entries, one is in metadata and second is
     * in g_fdb_info_set. Second one holds all dynamic entries learned from
     * interfaces. All learned entries are processed by metadata and propagated
     * from info set to metadata. But there may be short period of time that
     * those 2 sets will be out of sync untill notifications learned/aged will
     * be sent.
     */

    // Actually those sets should be in sync in VS except that existing one
    // can contain static entries crated by user, but it will contain all
    // entries !

    // we could have 2 sets static/dynamic and behave accordingly

    // TODO implement actual flush (9 cases) with ntf generation (queue)
    // also update meta db here

    // 1. get all fdb entries 
    // 2/ iterate through attributes and reduce number of entries
    // 3 generate notifications
    // 4 depending on config generate notification for flushed
    //  one by one or consolidated -
    // - for consolidated we need to generate at least static and dynamic entries
    //
    // we can right away update metadata here
    //
    //
    // metadata on sairedis needs to support types of consolidated fdb flush events like when vlan is 0 or port is zero
    // flush case needs to be corrected in metadata

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_flush_fdb_entries(
            switch_id,
            attr_count,
            attr_list,
            internal_vs_flush_fdb_entries);
}

VS_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t vs_fdb_api = {

    VS_GENERIC_QUAD_API(fdb_entry)

    vs_flush_fdb_entries,
};
