#include "client.h"
#include "error.h"
#include "sls.h"

#include <smpl.h>
#include <limits.h>

std::shared_ptr<smpl::Remote_Address> sls::global_server = nullptr;

sls::Client::Client(std::shared_ptr<smpl::Remote_Address> server){
    server_connection = std::unique_ptr<smpl::Channel>(server->connect());
}

std::vector<sls::Response> sls::Client::_request(const sls::Request &request){
    assert(request.key().size() > 0);

    std::vector<sls::Response> responses;
    std::string request_string;
    request.SerializeToString(&request_string);

    server_connection->send(request_string);
    for(;;){
        std::string returned = server_connection->recv();
        sls::Response retval;
        retval.ParseFromString(returned);
        responses.push_back(retval);
        if(retval.end_of_response()){
            break;
        }
        else{
            continue;
        }
    }
    return responses;
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::_interval(const std::string &key, const unsigned long long &start, const unsigned long long &end, const bool &is_time){
    assert(key.size() > 0);
    std::shared_ptr<std::deque<sls::Value> > r(new std::deque<sls::Value>);

    sls::Request request;
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    std::vector<sls::Response> responses = _request(request);

    for(const auto &response: responses){
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
    }

    return r;
}

bool sls::Client::append(const std::string &key, const std::string &data){
    assert(key.size() > 0);
    sls::Request request;
    request.set_key(key);

    //TODO: Can do this in one step?
    auto r = request.mutable_req_append();
    r->set_data(data);

    sls::Response retval = _request(request).front();

    return retval.success();
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::lastn(const std::string &key, const unsigned long long &num_entries){
    assert(key.size() > 0);
    return _interval(key, 1, num_entries, false);
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::all(const std::string &key){
    assert(key.size() > 0);
    return _interval(key, 1, ULONG_MAX, false);
}

std::shared_ptr< std::deque<sls::Value> > sls::Client::intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end){
    assert(key.size() > 0);
    return _interval(key, start, end, true);
}
