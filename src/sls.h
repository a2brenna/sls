#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<list>
#include<mutex>
#include<stack>

#include"sls.pb.h"

namespace sls{

    bool append(const char *key, std::string data);
    std::list<sls::Value> *lastn(const char *key, unsigned long long num_entries);
    std::list<sls::Value> *all(const char *key);
    std::list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
    std::string unwrap(sls::Value value);
    unsigned long long check_time(sls::Value value);

}

#endif
