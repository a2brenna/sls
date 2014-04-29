#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<list>

#include"sls.pb.h"

namespace sls{
    class SLS_Error{
        public:
            std::string msg;
            SLS_Error(std::string message);
    };
    void set_local_sls(bool new_val);
    bool append(const char *key, std::string data);
    std::list<sls::Value> *lastn(const char *key, unsigned long long num_entries);
    std::list<sls::Value> *all(const char *key);
    std::list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
    std::string unwrap(sls::Value value);
    unsigned long long check_time(sls::Value value);
}

#endif
