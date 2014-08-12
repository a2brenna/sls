#ifndef __SLS_SERVER_H__
#define __SLS_SERVER_H__

#include <memory>
#include <string>
#include <stack>
#include <mutex>
#include <list>

#include "sls.pb.h"

namespace sls{

    class Server{
        public:
            Server(std::string dd, unsigned long min, unsigned long max);
            ~Server();
            void handle(int sockfd);
            void handle_next_request();
            void sync();
        private:
            unsigned long cache_min;
            unsigned long cache_max;
            std::string disk_dir;

            std::map<std::string, std::list<sls::Value> > cache;
            std::map<std::string, std::mutex> locks;

            sls::Value wrap(std::string payload);
            void _page_out(std::string key, unsigned int skip);
            void _file_lookup(std::string key, std::string filename, sls::Archive *archive);
            void file_lookup(std::string key, std::string filename, std::list<sls::Value> *r);
            std::string next_lookup(std::string key, std::string filename);
            unsigned long long pick_time(const std::list<sls::Value> &d, unsigned long long start, unsigned long long end, std::list<sls::Value> *result);
            unsigned long long pick(const std::list<sls::Value> &d, unsigned long long current, unsigned long long start, unsigned long long end, std::list<sls::Value> *result);
            void _lookup(Socket *sock, sls::Request *request);
    };

}

#endif
