#include "syncd/ComparisonLogic.h"
#include "syncd/VidManager.h"

#include "meta/sai_serialize.h"

#include "swss/json.hpp"

#include "SaiSwitchAsic.h"

#include <map>
#include <unordered_map>
#include <fstream>

using namespace saiasiccmp;

using json = nlohmann::json;

class View
{
    public:

        // TODO support multiple switches

        View(
                _In_ const std::string& filename):
            m_maxObjectIndex(0)
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_NOTICE("loading view from: %s", filename.c_str());

            std::ifstream file(filename);

            if (!file.good())
            {
                SWSS_LOG_THROW("failed to open %s", filename.c_str());
            }

            json j;
            file >> j;

            loadVidRidMaps(j);
            loadAsicView(j);
            loadColdVids(j);
            loadHidden(j);

            for (auto& it: m_objTypeStrMap)
            {
                SWSS_LOG_NOTICE("%s: %zu", sai_serialize_object_type(it.first).c_str(), it.second.size());
            }
        }

        void loadVidRidMaps(
            _In_ const json& j)
        {
            SWSS_LOG_ENTER();

            json v2r = j["VIDTORID"].at("value");

            for (auto it = v2r.begin(); it != v2r.end(); it++)
            {
                std::string v = it.key();
                std::string r = it.value();

                sai_object_id_t vid;
                sai_object_id_t rid;

                sai_deserialize_object_id(v, vid);
                sai_deserialize_object_id(r, rid);

                m_vid2rid[vid] = rid;
                m_rid2vid[rid] = vid;

                auto ot = syncd::VidManager::objectTypeQuery(vid);

                m_oidTypeMap[ot].insert(vid);

                uint64_t index = syncd::VidManager::getObjectIndex(vid);

                m_maxObjectIndex = std::max(m_maxObjectIndex, index);
            }

            SWSS_LOG_NOTICE("oids: %zu\n", m_vid2rid.size());

            SWSS_LOG_NOTICE("max index: %lu (0x%lx), other max index: %lu (0x%lx)",
                    m_maxObjectIndex,
                    m_maxObjectIndex,
                    m_otherMaxObjectIndex,
                    m_otherMaxObjectIndex);

            if (m_oidTypeMap.at(SAI_OBJECT_TYPE_SWITCH).size() != 1)
            {
                SWSS_LOG_THROW("expected only 1 switch in view, FIXME");
            }

            m_switchVid = *m_oidTypeMap.at(SAI_OBJECT_TYPE_SWITCH).begin();

            m_switchRid = m_vid2rid.at(m_switchVid);
        }

        void loadAsicView(
                _In_ const json& j)
        {
            SWSS_LOG_ENTER();

            for (auto it = j.begin(); it != j.end(); it++)
            {
                std::string key = it.key();

                if (key.rfind("ASIC_STATE:") != 0)
                    continue;

                json vals = it.value().at("value");

                // skip ASIC_STATE
                key = key.substr(key.find_first_of(":") + 1);

                sai_object_meta_key_t mk;
                sai_deserialize_object_meta_key(key, mk);

                m_objTypeStrMap[mk.objecttype].insert(key);

                m_dump[key] = {}; // in case of NULL

                for (auto itt = vals.begin(); itt != vals.end(); itt++)
                {
                    if (itt.key() != "NULL")
                    {
                        m_dump[key][itt.key()] = itt.value();
                    }
                }
            }

            m_asicView = std::make_shared<syncd::AsicView>(m_dump);

            SWSS_LOG_NOTICE("view objects: %zu", m_asicView->m_soAll.size());
        }

        void loadColdVids(
                _In_ const json& j)
        {
            SWSS_LOG_ENTER();

            json cold = j["COLDVIDS"].at("value"); // TODO depend on switch

            for (auto it = cold.begin(); it != cold.end(); it++)
            {
                std::string v = it.key();
                std::string o = it.value();

                sai_object_id_t vid;
                sai_object_type_t ot;

                sai_deserialize_object_id(v, vid);
                sai_deserialize_object_type(o, ot);

                m_coldVids[vid] = ot;
            }

            SWSS_LOG_NOTICE("cold vids: %zu", m_coldVids.size());
        }

        void loadHidden(
                _In_ const json& j)
        {
            SWSS_LOG_ENTER();

            json hidden = j["HIDDEN"].at("value"); // TODO depend on switch

            for (auto it = hidden.begin(); it != hidden.end(); it++)
            {
                std::string h = it.key();
                std::string r = it.value();

                sai_object_id_t rid;

                sai_deserialize_object_id(r, rid);

                m_hidden[h] = rid;
            }

            SWSS_LOG_NOTICE("hidden: %zu", m_hidden.size());
        }

        void translateViewVids(
                _In_ uint64_t otherMaxObjectIndex)
        {
            SWSS_LOG_ENTER();

            // TODO use other views ?

            m_otherMaxObjectIndex = otherMaxObjectIndex;

            translateVidRidMaps();
            translateAsicView();
            translateColdVids();

            // no need for hidden, since they are RIDs
        }

        void translateVidRidMaps()
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_NOTICE("translating old VIDs to new VIDs");

            // TODO consider saving oids when rid/vid match both views

            m_oldVid2NewVid.clear();

            uint64_t index = std::max(m_maxObjectIndex, m_otherMaxObjectIndex) + 1;

            // translate except our starting point

            auto copy = m_vid2rid;

            m_vid2rid.clear();
            m_rid2vid.clear();

            m_oidTypeMap.clear();

            for (auto it: copy)
            {
                auto oldVid = it.first;
                auto rid = it.second;
                auto newVid = syncd::VidManager::updateObjectIndex(oldVid, index);

                index++;

                auto ot = syncd::VidManager::objectTypeQuery(oldVid);

                switch (ot)
                {
                    case SAI_OBJECT_TYPE_SWITCH:
                    case SAI_OBJECT_TYPE_PORT:
                    case SAI_OBJECT_TYPE_QUEUE:
                    case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
                    case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:

                        // don't translate starting point vids
                        newVid = oldVid;
                        break;

                    default:

                        SWSS_LOG_INFO("translating old VID %s to new VID %s",
                                sai_serialize_object_id(oldVid).c_str(),
                                sai_serialize_object_id(newVid).c_str());
                        break;
                }

                m_oldVid2NewVid[oldVid] = newVid;

                m_vid2rid[newVid] = rid;
                m_rid2vid[rid] = newVid;

                m_oidTypeMap[ot].insert(newVid);
            }
        }

        sai_object_id_t translateOldVidToNewVid(
                _In_ sai_object_id_t oldVid) const
        {
            SWSS_LOG_ENTER();

            if (oldVid == SAI_NULL_OBJECT_ID)
                return SAI_NULL_OBJECT_ID;

            auto it = m_oldVid2NewVid.find(oldVid);

            if (it == m_oldVid2NewVid.end())
            {
                SWSS_LOG_THROW("missing old vid %s", sai_serialize_object_id(oldVid).c_str());
            }

            SWSS_LOG_INFO("translating old vid %s", sai_serialize_object_id(oldVid).c_str());

            return m_oldVid2NewVid.at(oldVid);
        }

        void translateMetaKeyVids(
                _Inout_ sai_object_meta_key_t& mk) const
        {
            SWSS_LOG_ENTER();

            auto info = sai_metadata_get_object_type_info(mk.objecttype);

            if (info->isobjectid)
            {
                mk.objectkey.key.object_id = translateOldVidToNewVid(mk.objectkey.key.object_id);

                return;
            }

            // non object id, translate structure oids

            for (size_t j = 0; j < info->structmemberscount; ++j)
            {
                const sai_struct_member_info_t *m = info->structmembers[j];

                if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
                {
                    continue;
                }

                sai_object_id_t vid = m->getoid(&mk);

                vid = translateOldVidToNewVid(vid);

                m->setoid(&mk, vid);
            }
        }

        void translateAttrVids(
                _In_ const sai_attr_metadata_t* meta,
                _Inout_ sai_attribute_t& attr)
        {
            SWSS_LOG_ENTER();

            uint32_t count = 0;
            sai_object_id_t *objectIdList = NULL;

            switch (meta->attrvaluetype)
            {
                case SAI_ATTR_VALUE_TYPE_OBJECT_ID:

                    count = 1;
                    objectIdList = &attr.value.oid;

                    break;

                case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:

                    count = attr.value.objlist.count;
                    objectIdList = attr.value.objlist.list;

                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:

                    if (attr.value.aclfield.enable)
                    {
                        count = 1;
                        objectIdList = &attr.value.aclfield.data.oid;
                    }

                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

                    if (attr.value.aclfield.enable)
                    {
                        count = attr.value.aclfield.data.objlist.count;
                        objectIdList = attr.value.aclfield.data.objlist.list;
                    }

                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:

                    if (attr.value.aclaction.enable)
                    {
                        count = 1;
                        objectIdList = &attr.value.aclaction.parameter.oid;
                    }

                    break;

                case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                    if (attr.value.aclaction.enable)
                    {
                        count = attr.value.aclaction.parameter.objlist.count;
                        objectIdList = attr.value.aclaction.parameter.objlist.list;
                    }

                    break;

                default:

                    if (meta->isoidattribute)
                    {
                        SWSS_LOG_THROW("attribute %s is oid attrubute but not handled", meta->attridname);
                    }

                    // Attribute not contain any object ids.

                    break;
            }

            for (uint32_t i = 0; i < count; i++)
            {
                objectIdList[i] = translateOldVidToNewVid(objectIdList[i]);
            }
        }

        void translateAsicView()
        {
            SWSS_LOG_ENTER();

            m_objTypeStrMap.clear();

            swss::TableDump dump;

            for (auto it: m_dump)
            {
                std::string oldkey = it.first;

                sai_object_meta_key_t mk;
                sai_deserialize_object_meta_key(oldkey, mk);

                translateMetaKeyVids(mk);

                std::string key = sai_serialize_object_meta_key(mk);

                m_objTypeStrMap[mk.objecttype].insert(key);

                SWSS_LOG_INFO("translated %s to %s", oldkey.c_str(), key.c_str());

                dump[key] = {}; // in case of NULL

                // TODO translate oid attributes

                for (auto at: it.second)
                {
                    auto attrId = at.first;
                    auto attrOldVal = at.second;

                    syncd::SaiAttr attr(attrId, attrOldVal);

                    translateAttrVids(attr.getAttrMetadata(), *attr.getRWSaiAttr());

                    auto attrVal = sai_serialize_attr_value(*attr.getAttrMetadata(), *attr.getRWSaiAttr());

                    SWSS_LOG_INFO("translate %s: %s to %s", attrId.c_str(), attrOldVal.c_str(), attrVal.c_str());

                    dump[key][attrId] = attrVal;
                }
            }

            m_asicView = std::make_shared<syncd::AsicView>(dump);

            SWSS_LOG_NOTICE("view objects: %zu", m_asicView->m_soAll.size());
        }

        void translateColdVids()
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_NOTICE("translating cold VIDs");

            auto copy = m_coldVids;

            m_coldVids.clear();

            for (auto it: copy)
            {
                auto oldVid = it.first;
                auto ot = it.second;

                if (m_oldVid2NewVid.find(oldVid) == m_oldVid2NewVid.end())
                {
                    // some cold vids may be missing from rid vid map
                    // like vlan member or bridge port
                    continue;
                }

                m_coldVids[ m_oldVid2NewVid.at(oldVid) ] = ot;
            }

            SWSS_LOG_NOTICE("cold vids: %zu", m_coldVids.size());
        }


    public:

        uint64_t m_maxObjectIndex;
        uint64_t m_otherMaxObjectIndex;

        bool m_translateVids;

        swss::TableDump m_dump;

        sai_object_id_t m_switchVid;
        sai_object_id_t m_switchRid;

        std::map<sai_object_id_t, sai_object_id_t> m_vid2rid;
        std::map<sai_object_id_t, sai_object_id_t> m_rid2vid;

        std::map<sai_object_id_t, sai_object_id_t> m_oldVid2NewVid;

        std::map<sai_object_type_t, std::set<sai_object_id_t>> m_oidTypeMap;
        std::map<sai_object_type_t, std::set<std::string>> m_objTypeStrMap;
        
        std::shared_ptr<syncd::AsicView> m_asicView;

        std::map<sai_object_id_t, sai_object_type_t> m_coldVids;
        std::map<std::string, sai_object_id_t> m_hidden;
};

class ViewCmp
{
    public:

        ViewCmp(
                _In_ std::shared_ptr<View> a,
                _In_ std::shared_ptr<View> b):
            m_va(a),
            m_vb(b)
            {
                SWSS_LOG_ENTER();

                if (a->m_asicView->m_soAll.size() != b->m_asicView->m_soAll.size())
                {
                    SWSS_LOG_WARN("different number of objects in views %zu vs %zu",
                            a->m_asicView->m_soAll.size(),
                            b->m_asicView->m_soAll.size());
                }

                checkStartingPoint();
                checkVidRidMaps();
                checkHidden();

                // since second view can be translated, and some objects could
                // been removed (vlan members, bridge ports)
                // checkColdVids();
            }

        void checkColdVids()
        {
            SWSS_LOG_ENTER();

            // both cold vids should be the same, except when translated

            if (m_va->m_coldVids.size() != m_vb->m_coldVids.size())
            {
                SWSS_LOG_THROW("cold vids sizes differ: %zu vs %zu",
                        m_va->m_coldVids.size(),
                        m_vb->m_coldVids.size());
            }

            for (auto it: m_va->m_coldVids)
            {
                if (m_vb->m_coldVids.find(it.first) == m_vb->m_coldVids.end())
                {
                    SWSS_LOG_THROW("VID %s missing from second view", sai_serialize_object_id(it.first).c_str());
                }
            }
        }

        void checkHidden()
        {
            SWSS_LOG_ENTER();

            if (m_va->m_hidden.size() != m_vb->m_hidden.size())
            {
                SWSS_LOG_THROW("hidden size don't match");
            }

            for (auto it: m_va->m_hidden)
            {
                auto key = it.first;
                auto val = it.second;

                if (m_vb->m_hidden.find(key) == m_vb->m_hidden.end())
                {
                    SWSS_LOG_THROW("second view missing hidden %s", key.c_str());
                }

                if (m_vb->m_hidden.at(key) != val)
                {
                    SWSS_LOG_THROW("second view hidden %s value missmatch", key.c_str());
                }
            }
        }

        void checkVidRidMaps()
        {
            SWSS_LOG_ENTER();

            checkVidRidMaps(m_va, m_vb);
            checkVidRidMaps(m_vb, m_va);
        }

        void checkVidRidMaps(
                _In_ std::shared_ptr<View> a,
                _In_ std::shared_ptr<View> b)
        {
            SWSS_LOG_ENTER();

            for (auto it: a->m_vid2rid)
            {
                auto avid = it.first;
                auto arid = it.second;

                if (b->m_vid2rid.find(avid) != b->m_vid2rid.end() &&
                        b->m_vid2rid.at(avid) != arid)
                {
                    SWSS_LOG_THROW("vid %s exists, but have different rid value %s vs %s",
                            sai_serialize_object_id(avid).c_str(),
                            sai_serialize_object_id(arid).c_str(),
                            sai_serialize_object_id(b->m_vid2rid.at(avid)).c_str());
                }

                // this maybe not needed, we already checked starting point

                //if (b->m_rid2vid.find(arid) != b->m_rid2vid.end() &&
                //        b->m_rid2vid.at(arid) != avid)
                //{
                //    // TODO throw
                //    SWSS_LOG_WARN("rid %s exists, but have different vid value %s vs %s",
                //            sai_serialize_object_id(arid).c_str(),
                //            sai_serialize_object_id(avid).c_str(),
                //            sai_serialize_object_id(b->m_rid2vid.at(arid)).c_str());
                //}
            }
        }

        void checkStartingPoint()
        {
            SWSS_LOG_ENTER();

            // we assume at starting point vid/rid will match
            // on switches, ports, queues, scheduler groups, ipgs

            std::vector<sai_object_type_t> ot = {
                SAI_OBJECT_TYPE_SWITCH,
                SAI_OBJECT_TYPE_PORT,
                SAI_OBJECT_TYPE_QUEUE,
                SAI_OBJECT_TYPE_SCHEDULER_GROUP,
                SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
            };

            for (auto o: ot)
            {
                checkStartingPoint(o);
            }

            SWSS_LOG_NOTICE("starting point success");
        }

        void checkStartingPoint(
                _In_ sai_object_type_t ot)
        {
            SWSS_LOG_ENTER();

            if (m_va->m_oidTypeMap.size() != m_vb->m_oidTypeMap.size())
            {
                SWSS_LOG_THROW("different object %s count: %zu vs %zu",
                        sai_serialize_object_type(ot).c_str(),
                        m_va->m_oidTypeMap.size(),
                        m_vb->m_oidTypeMap.size());
            }

            for (auto vid: m_va->m_oidTypeMap.at(ot))
            {
                auto it = m_vb->m_vid2rid.find(vid);

                if (it == m_vb->m_vid2rid.end())
                {
                    SWSS_LOG_THROW("vid %s missing from second view",
                            sai_serialize_object_id(vid).c_str());
                }

                if (m_va->m_vid2rid.at(vid) != m_vb->m_vid2rid.at(vid))
                {
                    SWSS_LOG_THROW("vid %s has different RID values: %s vs %s",
                            sai_serialize_object_id(vid).c_str(),
                            sai_serialize_object_id(m_va->m_vid2rid.at(vid)).c_str(),
                            sai_serialize_object_id(m_vb->m_vid2rid.at(vid)).c_str());
                }
            }
        }

        bool compareViews()
        {
            SWSS_LOG_ENTER();

            auto breakConfig = std::make_shared<syncd::BreakConfig>();
            std::set<sai_object_id_t> initViewRemovedVids;

            auto sw = std::make_shared<SaiSwitchAsic>(
                    m_va->m_switchVid,
                    m_va->m_switchRid,
                    m_va->m_vid2rid,
                    m_va->m_rid2vid,
                    m_va->m_hidden,
                    m_va->m_coldVids);

            auto cl = std::make_shared<syncd::ComparisonLogic>(
                    nullptr, // m_vendorSai
                    sw,
                    nullptr, // handler
                    initViewRemovedVids,
                    m_va->m_asicView, // current
                    m_vb->m_asicView, // temp
                    breakConfig);

            cl->compareViews();

            // TODO support multiple asic views (multiple switch)

            if (m_va->m_asicView->asicGetOperationsCount())
            {
                SWSS_LOG_WARN("views are NOT EQUAL, operations count: %zu", m_va->m_asicView->asicGetOperationsCount());

                for (const auto &op: m_va->m_asicView->asicGetOperations())
                {
                    const std::string &key = kfvKey(*op.m_op);
                    const std::string &opp = kfvOp(*op.m_op);

                    SWSS_LOG_NOTICE("%s: %s", opp.c_str(), key.c_str());

                    const auto &values = kfvFieldsValues(*op.m_op);

                    for (auto &val: values)
                    {
                        SWSS_LOG_NOTICE("- %s %s", fvField(val).c_str(), fvValue(val).c_str());
                    }
                }

                return false;
            }

            SWSS_LOG_NOTICE("views are equal");

            return true;
        }

    public:

        std::shared_ptr<View> m_va;
        std::shared_ptr<View> m_vb;
};

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    //auto vendorSai = std::make_shared<syncd::VendorSai>();

    if (argc < 3)
    {
        printf("ERROR: add input files\n");
        exit(1);
    }

    auto a = std::make_shared<View>(argv[1]);
    auto b = std::make_shared<View>(argv[2]);

    // TODO - copy same rid/vid in both ? don't translate? - pass View to translate

    b->translateViewVids(a->m_maxObjectIndex);

    ViewCmp cmp(a, b);

    bool equal = cmp.compareViews();

    return equal ? EXIT_SUCCESS : EXIT_FAILURE;
}
