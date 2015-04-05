#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<deque>
#include<mutex>
#include<stack>
#include<smpl.h>

#include"sls.pb.h"

namespace sls{

    extern smpl::Remote_Address *global_server;

    bool append(const char *key, std::string data);
    std::deque<sls::Value> *lastn(const char *key, unsigned long long num_entries);
    std::deque<sls::Value> *all(const char *key);
    std::deque<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
    std::string unwrap(sls::Value value);
    unsigned long long check_time(sls::Value value);

}

#endif
