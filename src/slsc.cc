#include <deque>
#include <string>

#include "client.h"

#include "sls.h"
#include "sls.pb.h"

namespace sls{

bool append(const std::string &key, const std::string &data){
    assert(key.size() > 0);
    Client c(global_server);
    return c.append(key, data);
}

std::shared_ptr< std::deque<sls::Value> > lastn(const std::string &key, const unsigned long long &num_entries){
    assert(key.size() > 0);
    Client c(global_server);
    return c.lastn(key, num_entries);
}

std::shared_ptr< std::deque<sls::Value> > all(const std::string &key){
    assert(key.size() > 0);
    Client c(global_server);
    return c.all(key);
}

std::shared_ptr< std::deque<sls::Value> > intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end){
    assert(key.size() > 0);
    Client c(global_server);
    return c.intervalt(key, start, end);
}

std::string unwrap(const sls::Value &value){
    return value.data();
}
unsigned long long check_time(const sls::Value &value){
    return value.time();
}

}
