#include <deque>
#include <string>

#include "client.h"
#include "sls.h"
#include "sls.pb.h"

namespace sls{

bool append(const std::string &key, const std::string &data){
    assert(!key.empty());

    Client c(global_server);
    return c.append(key, data);
}

bool append(const std::string &key, const std::chrono::milliseconds &time, const std::string &data){
    assert(!key.empty());

    Client c(global_server);
    return c.append(key, time, data);
}

std::shared_ptr< std::deque<sls::Value> > lastn(const std::string &key, const unsigned long long &num_entries){
    assert(!key.empty());

    if( num_entries == 0){
        return std::shared_ptr<std::deque<sls::Value>>(new std::deque<sls::Value>());
    }

    Client c(global_server);
    return c.lastn(key, num_entries);
}

std::shared_ptr< std::deque<sls::Value> > all(const std::string &key){
    assert(!key.empty());

    Client c(global_server);
    return c.all(key);
}

std::shared_ptr< std::deque<sls::Value> > intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end){
    assert(!key.empty());
    assert(start <= end);

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
