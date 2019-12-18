#pragma once

#include "RemoteSaiInterface.h"

#include <memory>

#define SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(ot) \
    virtual sai_status_t remove(                                    \
            _In_ const sai_ ## ot ## _t* ot) override;

#define SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(ot) \
    virtual sai_status_t create(                                    \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _In_ const sai_attribute_t *attr_list) override;

#define SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(ot)    \
    virtual sai_status_t set(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ const sai_attribute_t *attr) override;

namespace sairedis
{
    /**
     * @brief Wrapper remote SAI interface.
     *
     * Class will wrap actual SAI implementation and it will provide recording
     * and other operations required on top of actual SAI interface
     * implementation.
     * 
     * At this points implementation can be done in any way.
     *
     * Wrapper should be used as metadata argument to make sure that all
     * arguments passed to wrapper are valid and correct.
     */
    class WrapperRemoteSaiInterface:
        public RemoteSaiInterface
    {
        public:

            WrapperRemoteSaiInterface(
                    _In_ std::shared_ptr<RemoteSaiInterface> impl);

            virtual ~WrapperRemoteSaiInterface() = default;

        public: // SAI interface overrides

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) override;

        public: // create ENTRY

            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(fdb_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(inseg_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(ipmc_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(l2mc_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(mcast_fdb_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(neighbor_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(route_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_CREATE_ENTRY(nat_entry);

        public: // remove ENTRY

            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(fdb_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(inseg_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(ipmc_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(l2mc_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(mcast_fdb_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(neighbor_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(route_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_REMOVE_ENTRY(nat_entry);

        public: // set ENTRY

            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(fdb_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(inseg_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(ipmc_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(l2mc_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(mcast_fdb_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(neighbor_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(route_entry);
            SAIREDIS_WRAPPERREMOTESAIINTERFACE_DECLARE_SET_ENTRY(nat_entry);

        private:

            std::shared_ptr<RemoteSaiInterface> m_implementation;
    };
}