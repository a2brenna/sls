#ifndef __SLS_CLIENT_H__
#define __SLS_CLIENT_H__

#include <hgutil/address.h>
#include <hgutil/socket.h>
#include <memory>

#include "sls.pb.h"

namespace sls{

    class Client{
        private:
            std::unique_ptr<Socket> server_connection;
            void request(const sls::Request &request, sls::Response *retva);
            std::list<sls::Value> *_interval(const char *key, unsigned long long start, unsigned long long end, bool ist_time);

        public:
            Client();
            Client(Address *server);

            bool append(const char *key, std::string data);
            std::list<sls::Value> *lastn(const char *key, unsigned long long num_entries);
            std::list<sls::Value> *all(const char *key);
            std::list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
            unsigned long long check_time(sls::Value value);
    };
}

#endif
