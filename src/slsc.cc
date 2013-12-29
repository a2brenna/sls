#include"sls.h"

#include <string>
#include <list>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <iostream>
#include "sls.pb.h"

#include <limits.h>

using namespace std;

bool sls_send(sls::Request request){
    int sockfd = 0;
    struct sockaddr_in serv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "Error : Could not create socket" << endl;
        return false;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(6998);

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       cerr << "Error : Connect Failed" << endl;
       return false;
    }

    string *rstring = (string *) malloc(sizeof (string));

    request.SerializeToString(rstring);

    bool retval;
    if (send(sockfd, rstring, rstring->length(), 0) == rstring->length()){
        retval = true;
    }
    else{
        cerr << "Failed to send entire request" << endl;
        retval = false;
    }

    free(rstring);
    return retval;
}

bool append(const char *key, string data){
    bool retval;
    try{
        sls::Request *request = (sls::Request *)(malloc (sizeof(sls::Request)));
        sls::Append *req_append = request->mutable_req_append();

        req_append->set_data(data);
        req_append->set_key(key);
        retval = sls_send(*request);
    }
    catch(...){
        return false;
    }

    return retval;
}

list<string> _interval(const char *key, unsigned long long start, unsigned long long end, bool is_time){
    list<string> r;
    //magic sauce
    return r;
}

list<string> lastn(const char *key, int num_entries){
    return intervaln(key, 0, num_entries - 1);
}

list<string> all(const char *key){
    return intervaln(key, 0, ULLONG_MAX);
}

list<string> intervaln(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, false);
}

list<string> intervalt(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, true);
}

string unwrap(const string value){
    string r;
    return r;
}
