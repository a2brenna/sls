#ifndef __SLS_SERVER_H__
#define __SLS_SERVER_H__

#include <string>
#include <mutex>
#include <list>

#include <smpl.h>

#include "sls.pb.h"

namespace sls{

    class Server {
        public:
            Server(std::string dd, unsigned long min, unsigned long max);
            virtual ~Server();

            void sync();
            void append(const std::string &key, const std::string &value);
            std::deque<sls::Value> index_lookup(const std::string &key, const size_t &start, const size_t &end);
            std::deque<sls::Value> time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &begin, const std::chrono::high_resolution_clock::time_point &end);

        private:
            unsigned long cache_min;
            unsigned long cache_max;
            std::string disk_dir;

            std::map<std::string, std::list<sls::Value> > cache;
            std::map<std::string, std::mutex> locks;

            sls::Value wrap(const std::string &payload);
            void _page_out(const std::string &key, const unsigned int &skip);
            void _file_lookup(const std::string &key, const std::string &filename, sls::Archive *archive);
            void file_lookup(const std::string &key, const std::string &filename, std::list<sls::Value> *r);
            std::string next_lookup(const std::string &key, const std::string &filename);
            unsigned long long pick_time(const std::list<sls::Value> &d, const unsigned long long &start, const unsigned long long &end, std::deque<sls::Value> *result);
            unsigned long long pick(const std::list<sls::Value> &d, unsigned long long current, const unsigned long long &start, const unsigned long long &end, std::deque<sls::Value> *result);
            std::deque<sls::Value> _lookup(const std::string &key, const unsigned long long &start, const unsigned long long &end, const bool &is_time);
    };

}

#endif
