#include "client.h"
#include "error.h"
#include "sls.h"

#include <smpl.h>
#include <limits.h>

smpl::Remote_Address* sls::global_server = nullptr;

sls::Client::Client(){

    //TODO: Maybe use unique_ptr for this and avoid this check?
    if(sls::global_server != nullptr){
        server_connection = std::unique_ptr<smpl::Channel>(sls::global_server->connect());
    }
    else{
        throw SLS_No_Server();
    }

}

sls::Client::Client(smpl::Remote_Address *server){
    server_connection = std::unique_ptr<smpl::Channel>(server->connect());
}

void sls::Client::_request(const sls::Request &request, sls::Response *retval){
    std::unique_ptr<std::string> request_string(new std::string);
    request.SerializeToString(request_string.get());

    server_connection->send(*request_string);

    std::unique_ptr<std::string> returned(new std::string);
    *returned = server_connection->recv();

    retval->ParseFromString(*returned);
    return;
}

std::deque<sls::Value>* sls::Client::_interval(const char *key, unsigned long long start, unsigned long long end, bool is_time){
    std::unique_ptr<std::deque<sls::Value> > r(new std::deque<sls::Value>);
    sls::Request request;
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    std::unique_ptr<sls::Response> response(new sls::Response);
    _request(request, response.get());

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

bool sls::Client::append(const char *key, std::string data){
    sls::Response retval;

    std::unique_ptr<sls::Request> request(new sls::Request);

    //TODO: Can do this in one step?
    request->mutable_req_append()->set_key(key);
    request->mutable_req_append()->set_data(data);

    _request(*request, &retval);

    return retval.success();
}

std::deque<sls::Value>* sls::Client::lastn(const char *key, unsigned long long num_entries){
    return _interval(key, 0, num_entries, false);
}

std::deque<sls::Value>* sls::Client::all(const char *key){
    return _interval(key, 0, ULONG_MAX, false);
}

std::deque<sls::Value>* sls::Client::intervalt(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, true);
}
