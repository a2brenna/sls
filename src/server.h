#ifndef __SLS_SERVER_H__
#define __SLS_SERVER_H__

#include <string>
#include <mutex>
#include <deque>
#include <memory>
#include "sls.pb.h"
#include "index.h"
#include "active_key.h"

namespace sls{

    class Fatal_Error {};

    class SLS {

        public:
            SLS(const std::string &d);

            void append(const std::string &key, const std::string &value);
            void append(const std::string &key, const std::chrono::milliseconds &time, const std::string &value);

            std::string index_lookup(const std::string &key, const size_t &start, const size_t &end);
            std::string time_lookup(const std::string &key, const uint64_t &begin, const uint64_t &end);
            std::string last_lookup(const std::string &key, const size_t &max_values);

        private:
            std::string _disk_dir;
            std::mutex _active_file_map_lock;
            std::map<std::string, std::shared_ptr<Active_Key>> _active_files;

            std::shared_ptr<Active_Key> _get_active_file(const std::string &key, const bool &create_if_missing);

    };

}

#endif
