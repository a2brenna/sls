#include "sls.h"
#include <iostream>
#include <sys/time.h>

using namespace std;
unsigned long long hires_time(){
    struct timeval tv;
    gettimeofday(&tv, NULL);

    unsigned long long millisecondsSinceEpoch =
    (unsigned long long)(tv.tv_sec) * 1000 +
    (unsigned long long)(tv.tv_usec) / 1000;

    return millisecondsSinceEpoch;
}

int main(int argc, char *argv[]){
    cerr << "Test Client Started... " << endl;
    string test_key = "key";
    string test_val = "value";

    append(test_key.c_str(), test_val);
    append(test_key.c_str(), "1");
    append(test_key.c_str(), "2");

    list<sls::Value> *r = all(test_key.c_str());
    cerr << "Got: " << r->size() << endl;
    for(list<sls::Value>::iterator i = r->begin(); i != r->end(); ++i){
        string d = unwrap(*i);
        cerr << d << endl;
    }
    for(list<sls::Value>::iterator i = r->begin(); i != r->end(); ++i){
        unsigned long long d = check_time(*i);
        cerr << d << endl;
    }

    r = lastn(test_key.c_str(), 6);
    cerr << "Got: " << r->size() << endl;
   
    unsigned long long time = hires_time();
    r = intervalt(test_key.c_str(), time - 60000, time);
    cerr << "Got intervalt: " << r->size() << endl;
    return 0;
}
