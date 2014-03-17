#include"sls.h"
#include <string>
#include <list>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include "sls.pb.h"
#include "hgutil.h"
#include <limits.h>
#include <memory>
#include <hgutil/raii.h>

using namespace std;

namespace sls{

void sls_send(sls::Request request, sls::Response *retval){
    retval->set_success(false);
    raii::FD sockfd(connect_to("127.0.0.1", "6998"));

    unique_ptr<string> rstring(new string);

    try{
        request.SerializeToString(rstring.get());
    }
    catch(...){
        retval->set_success(false);
        return;
    }

    //if (send(sockfd, rstring->c_str(), rstring->size(), MSG_NOSIGNAL) == rstring->size()){
    int sent = send(sockfd.get(), rstring->c_str(), rstring->size(), MSG_NOSIGNAL);

    if (sent > 0){
        if ((unsigned int)sent == rstring->size()){
            unique_ptr<string> returned(new string);
            read_sock(sockfd.get(), returned.get());
            if (returned->length() != 0){
                retval->ParseFromString(*returned);
                if(!retval->success()){
                    cerr << "Remote failure" << endl;
                }
            }
            else{
                cerr << "Failed to get response" << endl;
            }
        }
        else{
            cerr << "Failed to send entire request" << endl;
        }
    }
    else{
        cerr << "Error sending request" << endl;
    }
}

bool append(const char *key, string data){
    unique_ptr<sls::Response> retval(new sls::Response);
    try{
        unique_ptr<sls::Request> request(new sls::Request);

        request->mutable_req_append()->set_key(key);
        request->mutable_req_append()->set_data(data);

        sls_send(*request, retval.get());
    }
    catch(...){
        return false;
    }
    bool r = retval->success();
    return r;
}

list<sls::Value> *_interval(const char *key, unsigned long long start, unsigned long long end, bool is_time){
    unique_ptr<list<sls::Value> > r(new list<sls::Value>);
    sls::Request request;
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    unique_ptr<sls::Response> response(new sls::Response);
    sls_send(request, response.get());

    for(int i = 0; i < response->data_size(); i++){
        try{
            sls::Value foo;
            foo.ParseFromString((response->data(i)).data());
            r->push_back(foo);
        }
        catch(...){
            cerr << "Failed to parse incoming value" << endl;
        }
    }
    cerr << "sls fetched " << r->size() << endl;

    return r.release();
}

list<sls::Value> *lastn(const char *key, unsigned long long num_entries){
    return _interval(key, 0, num_entries, false);
}

list<sls::Value> *all(const char *key){
    return _interval(key, 0, ULONG_MAX, false);
}

list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, true);
}

string unwrap(sls::Value value){
    return value.data();
}
unsigned long long check_time(sls::Value value){
    return value.time();
}

}
