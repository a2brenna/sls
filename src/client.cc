#include "client.h"
#include "error.h"
#include "sls.h"

#include <smpl.h>
#include <limits.h>

std::shared_ptr<smpl::Remote_Address> sls::global_server = nullptr;

sls::Client::Client(){

    //TODO: Maybe use unique_ptr for this and avoid this check?
    if(sls::global_server != nullptr){
        server_connection = std::unique_ptr<smpl::Channel>(sls::global_server->connect());
    }
    else{
        throw SLS_No_Server();
    }

}

sls::Client::Client(std::shared_ptr<smpl::Remote_Address> server){
    server_connection = std::unique_ptr<smpl::Channel>(server->connect());
}

sls::Response sls::Client::_request(const sls::Request &request){
    std::string  request_string;
    request.SerializeToString(&request_string);

    server_connection->send(request_string);
    std::string returned = server_connection->recv();

    sls::Response retval;
    retval.ParseFromString(returned);
    return retval;
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::_interval(const char *key, const unsigned long long &start, const unsigned long long &end, const bool &is_time){
    std::shared_ptr<std::deque<sls::Value> > r(new std::deque<sls::Value>);

    sls::Request request;
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    sls::Response response = _request(request);

    for(int i = 0; i < response.data_size(); i++){
        try{
            sls::Value foo;
            foo.ParseFromString((response.data(i)).data());
            r->push_back(foo);
        }
        catch(...){
            throw SLS_Error("Failed to parse incoming value");
        }
    }

    return r;
}

bool sls::Client::append(const char *key, const std::string &data){
    sls::Request request;

    //TODO: Can do this in one step?
    request.mutable_req_append()->set_key(key);
    request.mutable_req_append()->set_data(data);

    sls::Response retval = _request(request);

    return retval.success();
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::lastn(const char *key, const unsigned long long &num_entries){
    return _interval(key, 0, num_entries, false);
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::all(const char *key){
    return _interval(key, 0, ULONG_MAX, false);
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::intervalt(const char *key, const unsigned long long &start, const unsigned long long &end){
    return _interval(key, start, end, true);
}
