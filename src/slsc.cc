#include <hgutil/raii.h>
#include <list>
#include <string>

#include "client.h"

#include "sls.h"
#include "sls.pb.h"

namespace sls{

bool append(const char *key, std::string data){
    Client c(global_server);
    return c.append(key, data);
}


std::list<sls::Value> *lastn(const char *key, unsigned long long num_entries){
    Client c(global_server);
    return c.lastn(key, num_entries);
}

std::list<sls::Value> *all(const char *key){
    Client c(global_server);
    return c.all(key);
}

std::list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end){
    Client c(global_server);
    return c.intervalt(key, start, end);
}

std::string unwrap(sls::Value value){
    return value.data();
}
unsigned long long check_time(sls::Value value){
    return value.time();
}

}
