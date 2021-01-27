#include "NetMsgRegistrar.h"

#include "swss/logger.h"
#include "swss/netdispatcher.h"
#include "swss/netlink.h"
#include "swss/select.h"

using namespace saivs;

#define MUTEX std::lock_guard<std::mutex> _lock(m_mutex);

NetMsgRegistrar::NetMsgRegistrar():
    m_index(0)
{
    SWSS_LOG_ENTER();

    m_run = true;

    m_thread = std::make_shared<std::thread>(&NetMsgRegistrar::run, this);
}

NetMsgRegistrar::~NetMsgRegistrar()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    m_run = false;

    m_link_thread_event.notify();

    m_thread->join();

    MUTEX;

    // this mutex makes sure that m_run flag is false when possible late
    // message arrive on callback

    SWSS_LOG_NOTICE("thread joined");

    SWSS_LOG_NOTICE("end");
}

NetMsgRegistrar& NetMsgRegistrar::getInstance()
{
    SWSS_LOG_ENTER();

    // Both registrar and dispatcher class are singleton class with static
    // local variable as singleton instance.  Registrar class depends on that
    // dispatcher instance to be valid at point where unregisterMessageHandler
    // method is called inside thread function. Since we don't know which
    // singleton was created first, it could happen that dispatcher singleton
    // could be initialized second. This will cause a problem, since this will
    // mean that that dispatcher singleton will be destroyed before registrar.
    // But since registrar is joining thread at the destructor, in this case it
    // will operate on already destructed dispatcher singleton which will lead
    // to double free or corruption error.
    //
    // To prevent this, we will instantiate dispatcher by calling getInstance()
    // to make sure that this singleton was created before registrar.
    //
    // For more refer to https://github.com/Azure/sonic-buildimage/issues/6466

    swss::NetDispatcher::getInstance();

    static NetMsgRegistrar instance;

    return instance;
}

uint64_t NetMsgRegistrar::registerCallback(
        _In_ Callback callback)
{
    SWSS_LOG_ENTER();

    MUTEX;

    m_map[m_index] = callback;

    return m_index++;
}

void NetMsgRegistrar::unregisterCallback(
        _In_ uint64_t index)
{
    SWSS_LOG_ENTER();

    MUTEX;

    auto it = m_map.find(index);

    if (it != m_map.end())
    {
        m_map.erase(it);
    }
}

void NetMsgRegistrar::unregisteraAll()
{
    SWSS_LOG_ENTER();

    MUTEX;

    m_map.clear();
}

void NetMsgRegistrar::resetIndex()
{
    SWSS_LOG_ENTER();

    MUTEX;
    
    m_index = 0;
}

void NetMsgRegistrar::run()
{
    SWSS_LOG_ENTER();

    swss::NetDispatcher::getInstance().registerMessageHandler(RTM_NEWLINK, this);
    swss::NetDispatcher::getInstance().registerMessageHandler(RTM_DELLINK, this);

    SWSS_LOG_NOTICE("netlink msg listener started");

    while (m_run)
    {
        try
        {
            swss::NetLink netlink;

            swss::Select s;

            netlink.registerGroup(RTNLGRP_LINK);
            netlink.dumpRequest(RTM_GETLINK);

            s.addSelectable(&netlink);
            s.addSelectable(&m_link_thread_event);

            while (m_run)
            {
                swss::Selectable *sel = NULL;

                int result = s.select(&sel);

                SWSS_LOG_INFO("select ended: %d", result);
            }
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("exception: %s", e.what());
            break;
        }
    }

    swss::NetDispatcher::getInstance().unregisterMessageHandler(RTM_NEWLINK);
    swss::NetDispatcher::getInstance().unregisterMessageHandler(RTM_DELLINK);

    SWSS_LOG_NOTICE("netlink msg listener ended");
}

void NetMsgRegistrar::onMsg(
        _In_ int nlmsg_type, 
        _In_ struct nl_object *obj)
{
    SWSS_LOG_ENTER();

    // this is async method

    MUTEX;

    if (!m_run)
    {
        SWSS_LOG_WARN("received message after sropping thread");
        return;
    }

    // since this message is received async it may happen that in this place
    // destructor was called and thread already joined, so we place MUTEX in
    // destructor ending to make sure that m_run is false if this happens

    // execute all callbacks under mutex

    for (auto& kvp: m_map)
    {
        kvp.second(nlmsg_type, obj);
    }
}
