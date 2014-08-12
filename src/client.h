#ifndef __SLS_CLIENT_H__
#define __SLS_CLIENT_H__

#include <hgutil/address.h>

namespace sls{

    Address *global_server = NULL;

    class Client{
        private:

        public:
            Client();
            Client(Address *server);
            ~Client();

            bool append(const char *key, std::string data);
            std::list<sls::Value> *lastn(const char *key, unsigned long long num_entries);
            std::list<sls::Value> *all(const char *key);
            std::list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
            unsigned long long check_time(sls::Value value);
    };
}

#endif
