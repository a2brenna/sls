#ifndef __SLS_SERVER_H__
#define __SLS_SERVER_H__

#include <string>
#include <mutex>
#include <deque>
#include "sls.pb.h"

namespace sls{

    class SLS {

        public:
            SLS(const std::string &d);
            virtual ~SLS();

            void append(const std::string &key, const std::string &value);
            std::deque<std::pair<uint64_t, std::string>> index_lookup(const std::string &key, const size_t &start, const size_t &end);
            std::deque<std::pair<uint64_t, std::string>> time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &begin, const std::chrono::high_resolution_clock::time_point &end);

            //Raw lookups
            std::string raw_index_lookup(const std::string &key, const size_t &start, const size_t &end);
            std::string raw_time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &begin, const std::chrono::high_resolution_clock::time_point &end);

        private:
            std::string disk_dir;
            std::mutex locks_lock;
            std::map<std::string, std::mutex> locks;

    };

}

#endif
