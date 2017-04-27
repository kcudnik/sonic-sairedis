#include <vector>
#include <string>
#include <iostream>
#include <sstream>

extern "C" {
#include <sai.h>
}

#include "swss/table.h"
#include "swss/logger.h"
#include "meta/saiserialize.h"
#include "sairedis.h"

#include <getopt.h>

using namespace swss;
using namespace std;

struct CmdOptions
{
    bool shortNames;
    bool disableColors;
    bool skipAttributes;
    bool followReferences;
    bool dumpTempView;
};

CmdOptions g_cmdOptions;
std::map<sai_object_id_t, const TableMap*> g_oid_map;

void printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: saidump [-C] [-s] [-h] [-t]" << std::endl;
    std::cout << "    -C --disableColors" << std::endl;
    std::cout << "        Disable colors" << std::endl;
    std::cout << "    -s --shortNames:" << std::endl;
    std::cout << "        Use short names" << std::endl;
    std::cout << "    -t --tempView:" << std::endl;
    std::cout << "        Dump temp view" << std::endl;
    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}

CmdOptions handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    CmdOptions options;

    options.shortNames = false;
    options.disableColors = false;
    options.dumpTempView = false;

    const char* const optstring = "Csht";

    while(true)
    {
        static struct option long_options[] =
        {
            { "disableColors",  no_argument,       0, 'C' },
            { "shortNames",     no_argument,       0, 's' },
            { "tempView",       no_argument,       0, 't' },
            { "help",           no_argument,       0, 'h' },
            { 0,                0,                 0,  0  }
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 't':
                SWSS_LOG_NOTICE("Dump temp vie");
                options.dumpTempView = true;
                break;

            case 'C':
                SWSS_LOG_NOTICE("Disable colors");
                options.disableColors = true;
                break;

            case 's':
                SWSS_LOG_NOTICE("Short names");
                options.shortNames = true;
                break;

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }

    return options;
}

size_t get_max_attr_len(const TableMap& map)
{
    SWSS_LOG_ENTER();

    size_t max = 0;

    for (const auto&field: map)
    {
        max = std::max(max, field.first.length());
    }

    return max;
}

std::string pad_string(std::string s, size_t pad)
{
    SWSS_LOG_ENTER();

    size_t len = s.length();

    if (len < pad)
    {
        s.insert(len, pad - len, ' ');
    }

    return s;
}

const TableMap* get_table_map(sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    auto it = g_oid_map.find(object_id);

    if (it == g_oid_map.end())
    {
        SWSS_LOG_ERROR("unable to find oid 0x%lx in oid map", object_id);
        throw;
    }

    return it->second;
}

void print_attributes(size_t indent, const TableMap& map)
{
    SWSS_LOG_ENTER();

    size_t max_len = get_max_attr_len(map);

    std::string str_indent = pad_string("", indent);

    for (const auto&field: map)
    {
        const sai_attr_metadata_t *meta;
        sai_deserialize_attr_id(field.first, &meta);

        std::stringstream ss;

        ss << str_indent << pad_string(field.first, max_len) << " : ";

        ss << field.second;

        std::cout << ss.str() << std::endl;
    }
}

int main(int argc, char ** argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    meta_init_db();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    g_cmdOptions = handleCmdLine(argc, argv);

    swss::DBConnector db(ASIC_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);

    std::string table = ASIC_STATE_TABLE;

    if (g_cmdOptions.dumpTempView)
    {
        table = TEMP_PREFIX + table;
    }

    swss::Table t(&db, table);

    TableDump dump;

    t.dump(dump);

    for (const auto&key: dump)
    {
        auto start = key.first.find_first_of(":");
        auto str_object_type = key.first.substr(0, start);
        auto str_object_id  = key.first.substr(start + 1);

        sai_object_type_t object_type;
        sai_deserialize_object_type(str_object_type, object_type);

        auto info = sai_all_object_type_infos[object_type];

        if (!info->isnonobjectid)
        {
            sai_object_id_t object_id;
            sai_deserialize_object_id(str_object_id, object_id);

            g_oid_map[object_id] = &key.second;
        }
    }

    for (const auto&key: dump)
    {
        auto start = key.first.find_first_of(":");
        auto str_object_type = key.first.substr(0, start);
        auto str_object_id  = key.first.substr(start + 1);

        sai_object_type_t object_type;
        sai_deserialize_object_type(str_object_type, object_type);

        if (g_cmdOptions.shortNames)
        {
            str_object_type = metadata_enum_sai_object_type_t.valuesshortnames[object_type];
        }

        std::cout << str_object_type << " " << str_object_id << " " << std::endl;

        auto info = sai_all_object_type_infos[object_type];

        switch (object_type)
        {
            case SAI_OBJECT_TYPE_FDB_ENTRY:
                {
                    sai_fdb_entry_t fdb_entry;
                    sai_deserialize_fdb_entry(str_object_id, fdb_entry);
                }
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                {
                    sai_neighbor_entry_t neighbor_entry;
                    sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);
                }
                break;

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                {
                    sai_route_entry_t route_entry;
                    sai_deserialize_route_entry(str_object_id, route_entry);
                }
                break;

            default:

                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("object type %s is not supported yet", info->objecttypename);
                }

                {
                    sai_object_id_t object_id;
                    sai_deserialize_object_id(str_object_id, object_id);
                }
                break;
        }

        size_t indent = 4;

        print_attributes(indent, key.second);

        std::cout << std::endl;
    }
}
