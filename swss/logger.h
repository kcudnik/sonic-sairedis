#ifndef SWSS_COMMON_LOGGER_H
#define SWSS_COMMON_LOGGER_H

#include <string>
#include <chrono>
#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>

namespace swss {

#define SWSS_LOG_ERROR(MSG, ...)       
#define SWSS_LOG_WARN(MSG, ...)        
#define SWSS_LOG_NOTICE(MSG, ...)      
#define SWSS_LOG_INFO(MSG, ...)        
#define SWSS_LOG_DEBUG(MSG, ...)       

#define SWSS_LOG_ENTER()               
#define SWSS_LOG_TIMER(msg, ...)       

#define SWSS_LOG_THROW(MSG, ...)    throw

}

#endif /* SWSS_COMMON_LOGGER_H */
