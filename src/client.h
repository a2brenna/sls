#ifndef __SLS_CLIENT_H__
#define __SLS_CLIENT_H__

#include <smpl.h>
#include <memory>

#include "sls.h"
#include "sls.pb.h"

namespace sls{

    extern std::shared_ptr<smpl::Remote_Address> global_server;

    class Client{
        private:
            std::unique_ptr<smpl::Channel> server_connection;
            void _request(const sls::Request &request, sls::Response *retval);
            std::deque<sls::Value> *_interval(const char *key, const unsigned long long &start, const unsigned long long &end, const bool &is_time);

        public:
            Client();
            Client(std::shared_ptr<smpl::Remote_Address> server);

            bool append(const char *key, const std::string &data);
            std::deque<sls::Value> *lastn(const char *key, const unsigned long long &num_entries);
            std::deque<sls::Value> *all(const char *key);
            std::deque<sls::Value> *intervalt(const char *key, const unsigned long long &start, const unsigned long long &end);
            unsigned long long check_time(const sls::Value &value);
    };
}

#endif
