#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<list>

#include"sls.pb.h"

using namespace std;
namespace sls{
    bool append(const char *key, string data);
    list<sls::Value> *lastn(const char *key, unsigned long long num_entries);
    list<sls::Value> *all(const char *key);
    list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
    string unwrap(sls::Value value);
    unsigned long long check_time(sls::Value value);
}

#endif
