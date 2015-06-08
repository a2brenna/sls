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

    bool append(const std::string &key, const std::string &data);
    std::shared_ptr< std::deque<sls::Value> > lastn(const std::string &key, const unsigned long long &num_entries);
    std::shared_ptr< std::deque<sls::Value> > all(const std::string &key);
    std::shared_ptr< std::deque<sls::Value> > intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end);
    std::string unwrap(const sls::Value &value);
    unsigned long long check_time(const sls::Value &value);

}

#endif
