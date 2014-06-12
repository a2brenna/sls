#include <iostream>
#include <sys/time.h>
#include <hgutil.h>

#include "sls.h"

int main(){
    std::cerr << "Test Client Started... " << std::endl;
    std::string test_key = "key";
    std::string test_val = "value";

    sls::append(test_key.c_str(), test_val);
    sls::append(test_key.c_str(), "1");
    sls::append(test_key.c_str(), "2");

    std::list<sls::Value> *r = sls::all(test_key.c_str());
    std::cerr << "Got: " << r->size() << std::endl;
    for(std::list<sls::Value>::iterator i = r->begin(); i != r->end(); ++i){
        std::string d = unwrap(*i);
        std::cerr << d << std::endl;
    }
    for(std::list<sls::Value>::iterator i = r->begin(); i != r->end(); ++i){
        unsigned long long d = sls::check_time(*i);
        std::cerr << d << std::endl;
    }

    r = sls::lastn(test_key.c_str(), 6);
    std::cerr << "Got: " << r->size() << std::endl;
   
    unsigned long long time = milli_time();
    r = sls::intervalt(test_key.c_str(), time - 60000, time);
    std::cerr << "Got sls::intervalt: " << r->size() << std::endl;
    return 0;
}
