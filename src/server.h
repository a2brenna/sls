#ifndef __SLS_SERVER_H__
#define __SLS_SERVER_H__

#include <string>
#include <mutex>
#include <deque>
#include "sls.pb.h"
#include "index.h"

namespace sls{

    class SLS {

        public:
            SLS(const std::string &d);
            virtual ~SLS();

            void append(const std::string &key, const std::string &value);

            //Raw lookups
            std::string index_lookup(const std::string &key, const size_t &start, const size_t &end);
            std::string time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &begin, const std::chrono::high_resolution_clock::time_point &end);
            std::string last_lookup(const std::string &key, const size_t &max_values);

        private:
            std::string disk_dir;
            std::mutex maps_lock;
            std::map<std::string, std::mutex> disk_locks;
            std::map<std::string, Index> indices;
            std::map<std::string, std::string> active_files;

            std::vector<Index_Record> _index_time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end);
            std::vector<Index_Record> _index_position_lookup(const std::string &key, const uint64_t &start, const uint64_t &end);

    };

}

#endif
