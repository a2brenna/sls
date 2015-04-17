#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<deque>
#include<mutex>
#include<stack>
#include<memory>
#include<smpl.h>

#include"sls.pb.h"

namespace sls{

    extern std::shared_ptr<smpl::Remote_Address> global_server;

    bool append(const char *key, const std::string &data);
    std::deque<sls::Value> *lastn(const char *key, const unsigned long long &num_entries);
    std::deque<sls::Value> *all(const char *key);
    std::deque<sls::Value> *intervalt(const char *key, const unsigned long long &start, const unsigned long long &end);
    std::string unwrap(const sls::Value &value);
    unsigned long long check_time(const sls::Value &value);

}

#endif
