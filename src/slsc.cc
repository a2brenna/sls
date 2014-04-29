#include "sls.h"
#include <string>
#include <list>
#include "sls.pb.h"
#include "hgutil.h"
#include <limits.h>
#include <memory>
#include <hgutil/raii.h>

using namespace std;

namespace sls{

SLS_Error::SLS_Error(string message){
    msg = message;
}

bool local_sls = false;

void set_local_sls(bool new_val){
    local_sls = new_val;
}

int _get_socket(){
    int sockfd = -1;
    if(!local_sls){
        sockfd = connect_to("127.0.0.1", 6998, false);
    }
    else{
        sockfd = connect_to("/tmp/sls.sock", false);
    }
    return sockfd;
}

void sls_send(sls::Request request, sls::Response *retval){
    retval->set_success(false);
    try{
        raii::FD sockfd(_get_socket());

        unique_ptr<string> rstring(new string);

        try{
            request.SerializeToString(rstring.get());
        }
        catch(...){
            retval->set_success(false);
            throw SLS_Error("Failed to serialize request");
        }

        if(!send_string(sockfd.get(), *rstring)){
            retval->set_success(false);
            throw SLS_Error("Failed to sens sls request");
        }

        unique_ptr<string> returned(new string);
        if(!recv_string(sockfd.get(), *returned)){
            throw SLS_Error("Failed to receive sls response");
        }
        retval->ParseFromString(*returned);
    }
    catch(...){
        throw SLS_Error("Unknown error in sls_send");
    }
    return;
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
            throw SLS_Error("Failed to parse incoming value");
        }
    }

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
