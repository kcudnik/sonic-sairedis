#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/table.h"

#include <string>
#include <vector>

namespace sairedis
{
    class Recorder
    {
        public:

            Recorder();

            virtual ~Recorder();

        public: // SAI API

            // Ideally we would have each record api here with exact the same
            // attributes as SAI apis, but since we use serialize methods for
            // attributes and the same format is used in recorder and in REDIS
            // database in message queue, it make sense to reuse those
            // serialized values to not double serialize the same attributes.

            // generic create, remove, set, get
            // generic bulk apis
            //
            // sai_flush_fdb_entries
            // sai_remove_all_neighbor_entries
            // sai_clear_port_all_stats
            //
            // sai_api_query
            // sai_log_set
            // sai_api_initialize
            // sai_api_uninitialize
            // sai_dbg_generate_dump
            // sai_object_type_get_availability
            // sai_get_maximum_attribute_count
            // sai_get_object_key
            // sai_get_object_count
            // sai_query_attribute_capability
            // sai_query_attribute_enum_values_capability
            // sai_bulk_get_attribute
            // sai_tam_telemetry_get_data

        public: // SAI quad API

            void recordGenericCreate(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordGenericCreateResponse(
                    _In_ sai_status_t status);

            void recordGenericRemove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId);

            void recordGenericRemove(
                    _In_ const std::string& key);

            void recordGenericRemoveResponse(
                    _In_ sai_status_t status);

            void recordGenericSet(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordGenericSetResponse(
                    _In_ sai_status_t status);

            void recordGenericGet(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordGenericGetResponse(
                    _In_ sai_status_t status,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

        public: // SAI stats API

            void recordGenericGetStats(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordGenericGetStatsResponse(
                    _In_ sai_status_t status,
                    _In_ uint32_t count,
                    _In_ const uint64_t *counters);

            void recordGenericClearStats(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordGenericClearStatsResponse(
                    _In_ sai_status_t status);

        public: // SAI bulk API

            void recordBulkGenericCreate(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordBulkGenericCreateResponse(
                    _In_ sai_status_t status,
                    _In_ uint32_t objectCount,
                    _In_ const sai_status_t *objectStatuses);

            void recordBulkGenericRemove(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordBulkGenericRemoveResponse(
                    _In_ sai_status_t status,
                    _In_ uint32_t objectCount,
                    _In_ const sai_status_t *objectStatuses);

            void recordBulkGenericSet(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordBulkGenericSetResponse(
                    _In_ sai_status_t status,
                    _In_ uint32_t objectCount,
                    _In_ const sai_status_t *objectStatuses);

        public: // SAI query interface API

            void recordFlushFdbEntries(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordFlushFdbEntriesResponse(
                    _In_ sai_status_t status);

        public: // SAI global interface API

            void recordQueryAttributeEnumValuesCapability(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordQueryAttributeEnumValuesCapabilityResponse(
                    _In_ sai_status_t status,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordObjectTypeGetAvailability(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

            void recordObjectTypeGetAvailabilityResponse(
                    _In_ sai_status_t status,
                    _In_ const std::vector<swss::FieldValueTuple>& arguments);

        public: // SAI notifications

            void recordNotification(
                    _In_ const std::string &name,
                    _In_ const std::string &serializedNotification,
                    _In_ const std::vector<swss::FieldValueTuple> &values);

        public: // sairedis/syncd internal API

            void recordNotifySyncd(
                    _In_ const std::string& key);

            void recordNotifySyncdResponse(
                    _In_ sai_status_t status);

        public: // Recorder API

            void enableRecording(
                    _In_ bool enabled);

            bool setRecordingOutputDirectory(
                    _In_ const sai_attribute_t &attr);

            void requestLogRotate();

        public: // static helper functions

            static std::string getTimestamp();

        private:

            void recordingFileReopen();

            void startRecording();

            void stopRecording();

            void recordLine(
                    _In_ const std::string& line);

        private:

            bool m_performLogRotate;

            bool m_enabled;

            std::string m_recordingOutputDirectory;

            std::string m_recordingFileName;

            std::string m_recordingFile;

            std::ofstream m_ofstream;

            std::mutex m_mutex;
    };
}
