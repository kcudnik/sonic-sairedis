#include "sai_vs.h"

#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(DST_TAG_ENTRY, dst_tag_entry);
VS_BULK_CREATE_ENTRY_EX(DST_TAG_ENTRY, dst_tag_entry, dst_tag_entries);
VS_BULK_REMOVE_ENTRY_EX(DST_TAG_ENTRY, dst_tag_entry, dst_tag_entries);

VS_GENERIC_QUAD_ENTRY(SRC_TAG_ENTRY, src_tag_entry);
VS_BULK_CREATE_ENTRY_EX(SRC_TAG_ENTRY, src_tag_entry, src_tag_entries);
VS_BULK_REMOVE_ENTRY_EX(SRC_TAG_ENTRY, src_tag_entry, src_tag_entries);

const sai_dash_tag_api_t vs_dash_tag_api = {

    VS_GENERIC_QUAD_API(dst_tag_entry)
    vs_bulk_create_dst_tag_entries,
    vs_bulk_remove_dst_tag_entries,

    VS_GENERIC_QUAD_API(src_tag_entry)
    vs_bulk_create_src_tag_entries,
    vs_bulk_remove_src_tag_entries,
};
