#pragma once

extern "C" {
#include "sai.h"
}

//#include "SaiInterface.h"
//#include "VirtualOidTranslator.h"
//
#include <set>
//#include <string>
#include <unordered_map>
//#include <vector>
//#include <map>
//#include <memory>

namespace syncd
{
    class SaiSwitchInterface
    {
        private:

            SaiSwitchInterface(const SaiSwitchInterface&);
            SaiSwitchInterface& operator=(const SaiSwitchInterface&);

        public:

            SaiSwitchInterface(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t switchRid);

            virtual ~SaiSwitchInterface() = default;

        public:

            sai_object_id_t getVid() const;
            sai_object_id_t getRid() const;

        public:

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const = 0;

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const = 0;

            virtual bool isDiscoveredRid(
                    _In_ sai_object_id_t rid) const  = 0;

            virtual bool isColdBootDiscoveredRid(
                    _In_ sai_object_id_t rid) const  = 0;

            virtual bool isSwitchObjectDefaultRid(
                    _In_ sai_object_id_t rid) const  = 0;

            virtual bool isNonRemovableRid(
                    _In_ sai_object_id_t rid) const  = 0;

            virtual std::set<sai_object_id_t> getDiscoveredRids() const  = 0;

            virtual sai_object_id_t getSwitchDefaultAttrOid(
                    _In_ sai_attr_id_t attr_id) const  = 0;

            virtual void removeExistingObject(
                    _In_ sai_object_id_t rid)  = 0;

            virtual void removeExistingObjectReference(
                    _In_ sai_object_id_t rid)  = 0;

            virtual void getDefaultMacAddress(
                    _Out_ sai_mac_t& mac) const  = 0;

            virtual sai_object_id_t getDefaultValueForOidAttr(
                    _In_ sai_object_id_t rid,
                    _In_ sai_attr_id_t attr_id)  = 0;

            virtual std::set<sai_object_id_t> getColdBootDiscoveredVids() const  = 0;

            virtual std::set<sai_object_id_t> getWarmBootDiscoveredVids() const  = 0;

            virtual void onPostPortCreate(
                    _In_ sai_object_id_t port_rid,
                    _In_ sai_object_id_t port_vid)  = 0;

            virtual void postPortRemove(
                    _In_ sai_object_id_t portRid)  = 0;

            virtual void collectPortRelatedObjects(
                    _In_ sai_object_id_t portRid)  = 0;

        protected:

            /**
             * @brief Switch virtual ID assigned by syncd.
             */
            sai_object_id_t m_switch_vid;

            /**
             * @brief Switch real ID assigned by SAI SDK.
             */
            sai_object_id_t m_switch_rid;
    };
}
