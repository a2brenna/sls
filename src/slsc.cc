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

sls::Response *sls_send(sls::Request request){
    struct sockaddr_in serv_addr;

    sls::Response *retval = new sls::Response;
    retval->set_success(false);

    struct addrinfo hints, *res;
    int sockfd;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo("127.0.0.1", "6998", &hints, &res);

    // make a socket:

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // connect!

    connect(sockfd, res->ai_addr, res->ai_addrlen);

    string *rstring = new string;

    try{
        request.SerializeToString(rstring);
    }
    catch(...){
        retval->set_success(false);
        return retval;
    }

    if (send(sockfd, rstring->c_str(), rstring->length(), 0) == rstring->length()){
        string *returned = new string;
        int i = 0;
        char b[512];
        do{
            bzero(b,512);
            i = recv(sockfd, b, 512, 0);
            returned->append(b, i);
        }
        while(i == 512);

        retval->ParseFromString(*returned);
        delete returned;
    }
    else{
        cerr << "Failed to send entire request" << endl;
    }
    delete rstring;

    return retval;
}

bool append(const char *key, string data){
    sls::Response *retval;
    try{
        sls::Request *request = new sls::Request;

        request->mutable_req_append()->set_key(key);
        request->mutable_req_append()->set_data(data);

        retval = sls_send(*request);
    }
    catch(...){
        return false;
    }

    bool r = retval->success();
    free(retval);
    return r;
}

list<string> *_interval(const char *key, unsigned long long start, unsigned long long end, bool is_time){
    list<string> *r = new list<string>;
    sls::Request request;
    sls::Range *range = request.mutable_req_range();
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    sls::Response *response = sls_send(request);

    for(int i = 0; i < response->data_size(); i++){
        r->push_back( (response->data(i)).data() );
    }

    free(response);
    return r;
}

list<string> *lastn(const char *key, int num_entries){
    return intervaln(key, 0, num_entries - 1);
}

list<string> *all(const char *key){
    return intervaln(key, 0, ULLONG_MAX);
}

list<string> *intervaln(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, false);
}

list<string> *intervalt(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, true);
}

string unwrap(const string value){
    string r;
    return r;
}
