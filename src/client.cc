#include "client.h"

#include <hgutil/address.h>
#include <hgutil/socket.h>
#include <hgutil/fd.h>
#include <limits.h>

Address* sls::global_server = NULL;

sls::Client::Client(){

    //TODO: Maybe use unique_ptr for this and avoid this check?
    if(global_server != NULL){
        server_connection(new Raw_Socket(connect_to(global_server)));
    }
    else{
        throw SLS_No_Server();
    }

}

Client::Client(Address *server){
    server_connection(new Raw_Socket(connect_to(global_server)));
}

void Client::request(const sls::Request &request, sls::Response *retval){
    std::unique_ptr<std::string> request_string(new std::string);
    request.SerializeToString(*request_string);

    server_connection->send_string();

    std::unique_ptr<std::string> returned(new std::string);
    server_connection->recv_string(returned.get());

    retval->ParseFromString(*returned);
    return;
}

std::list<sls::Value>* Client::_interval(const char *key, unsigned long long start, unsigned long long end, bool is_time){
    std::unique_ptr<std::list<sls::Value> > r(new std::list<sls::Value>);
    sls::Request request;
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    std::unique_ptr<sls::Response> response(new sls::Response);
    request(request, response.get());

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

bool Client::append(const char *key, std::string data){
    sls::Response retval;

    std::unique_ptr<sls::Request> request(new sls::Request);

    //TODO: Can do this in one step?
    request->mutable_req_append()->set_key(key);
    request->mutable_req_append()->set_data(key);

    request(*request, &retval);

    return retval->success();
}

std::list<sls::Value>* Client::lastn(const char *key, unsigned long long num_entries){
    return _interval(key, 0, num_entries, false);
}

std::list<sls::Value>* Client::all(const char *key){
    return _interval(key, 0, ULONG_MAX, false);
}

std::list<sls::Value>* Client::intervalt(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, true);
}
