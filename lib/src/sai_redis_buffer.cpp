#include "sai_redis.h"

/**
 * @brief Create ingress priority group
 *
 * @param[out] ingress_pg_id Ingress priority group
 * @param[in] switch_id Switch id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_create_ingress_priority_group(
        _Out_ sai_object_id_t* ingress_pg_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
            ingress_pg_id,
            switch_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * @brief Remove ingress priority group
 *
 * @param[in] ingress_pg_id Ingress priority group
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_remove_ingress_priority_group(
        _In_ sai_object_id_t ingress_pg_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
            ingress_pg_id,
            &redis_generic_remove);
}

/**
 * @brief Set ingress priority group attribute
 *
 * @param[in] ingress_pg_id Ingress priority group id
 * @param[in] attr Attribute to set
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_set_ingress_priority_group_attribute(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
            ingress_pg_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get ingress priority group attributes
 *
 * @param[in] ingress_pg_id Ingress priority group id
 * @param[in] attr_count Number of attributes
 * @param[inout] attr_list Array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_get_ingress_priority_group_attribute(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
            ingress_pg_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief Get ingress priority group statistics counters.
 *
 * @param[in] ingress_pg_id Ingress priority group id
 * @param[in] counter_ids Specifies the array of counter ids
 * @param[in] number_of_counters Number of counters in the array
 * @param[out] counters Array of resulting counter values.
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_get_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ const sai_ingress_priority_group_stat_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Clear ingress priority group statistics counters.
 *
 * @param[in] ingress_pg_id Ingress priority group id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_clear_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_ingress_priority_group_stat_t *counter_ids)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Create buffer pool
 *
 * @param[out] pool_id Buffer pool id
 * @param[in] switch_id Switch id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_create_buffer_pool(
        _Out_ sai_object_id_t* pool_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            switch_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * @brief Remove buffer pool
 *
 * @param[in] pool_id Buffer pool id
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_remove_buffer_pool(
        _In_ sai_object_id_t pool_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            &redis_generic_remove);
}

/**
 * @brief Set buffer pool attribute
 *
 * @param[in] pool_id Buffer pool id
 * @param[in] attr Attribute
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_set_buffer_pool_attribute(
        _In_ sai_object_id_t pool_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get buffer pool attributes
 *
 * @param[in] pool_id Buffer pool id
 * @param[in] attr_count Number of attributes
 * @param[inout] attr_list Array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_get_buffer_pool_attribute(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief Get buffer pool statistics counters.
 *
 * @param[in] pool_id Buffer pool id
 * @param[in] counter_ids Specifies the array of counter ids
 * @param[in] number_of_counters Number of counters in the array
 * @param[out] counters Array of resulting counter values.
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_get_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ const sai_buffer_pool_stat_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Clear buffer pool statistics counters.
 *
 * @param[in] pool_id Buffer pool id
 * @param[in] number_of_counters Number of counters in the array
 * @param[in] counter_ids Specifies the array of counter ids
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_clear_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_buffer_pool_stat_t *counter_ids)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Create buffer profile
 *
 * @param[out] buffer_profile_id Buffer profile id
 * @param[in] switch_id Switch object id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_create_buffer_profile(
        _Out_ sai_object_id_t *buffer_profile_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            switch_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * @brief Remove buffer profile
 *
 * @param[in] buffer_profile_id Buffer profile id
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_remove_buffer_profile(
        _In_ sai_object_id_t buffer_profile_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            &redis_generic_remove);
}

/**
 * @brief Set buffer profile attribute
 *
 * @param[in] buffer_profile_id Buffer profile id
 * @param[in] attr Attribute
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_set_buffer_profile_attribute(
        _In_ sai_object_id_t buffer_profile_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get buffer profile attributes
 *
 * @param[in] buffer_profile_id Buffer profile id
 * @param[in] attr_count Number of attributes
 * @param[inout] attr_list Array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t redis_get_buffer_profile_attribute(
        _In_ sai_object_id_t buffer_profile_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief buffer methods table retrieved with sai_api_query()
 */
const sai_buffer_api_t redis_buffer_api = {
    redis_create_buffer_pool,
    redis_remove_buffer_pool,
    redis_set_buffer_pool_attribute,
    redis_get_buffer_pool_attribute,
    redis_get_buffer_pool_stats,
    redis_clear_buffer_pool_stats,
    redis_create_ingress_priority_group,
    redis_remove_ingress_priority_group,
    redis_set_ingress_priority_group_attribute,
    redis_get_ingress_priority_group_attribute,
    redis_get_ingress_priority_group_stats,
    redis_clear_ingress_priority_group_stats,
    redis_create_buffer_profile,
    redis_remove_buffer_profile,
    redis_set_buffer_profile_attribute,
    redis_get_buffer_profile_attribute,
};
