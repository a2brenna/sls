#include "client.h"
#include "error.h"
#include "sls.h"
#include "archive.h"

#include <smpl.h>
#include <limits.h>

std::shared_ptr<smpl::Remote_Address> sls::global_server = nullptr;

sls::Client::Client(std::shared_ptr<smpl::Remote_Address> server){
    server_connection = std::unique_ptr<smpl::Channel>(server->connect());
}

std::pair<sls::Response, std::vector<std::pair<uint64_t, std::string>>> sls::Client::_request(const sls::Request &request){
    assert(request.key().size() > 0);

    std::string request_string;
    request.SerializeToString(&request_string);

    server_connection->send(request_string);
    std::string returned = server_connection->recv();

    sls::Response response;
    response.ParseFromString(returned);

    std::vector<std::pair<uint64_t, std::string>> data_vector;

    if(response.data_to_follow()){
        const Archive data(server_connection->recv());
        data_vector = data.extract();
    }
    return std::pair<sls::Response, std::vector<std::pair<uint64_t, std::string>>>(response, data_vector);
}

std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > sls::Client::_interval(const std::string &key, const unsigned long long &start, const unsigned long long &end, const bool &is_time){
    assert(key.size() > 0);
    std::shared_ptr<std::deque<std::pair<std::chrono::milliseconds, std::string>> > result_deque(new std::deque<std::pair<std::chrono::milliseconds, std::string>>);

    sls::Request request;
    request.mutable_req_range()->set_start(start);
    request.mutable_req_range()->set_end(end);
    request.mutable_req_range()->set_is_time(is_time);
    request.set_key(key);

    std::pair<sls::Response, std::vector<std::pair<uint64_t, std::string>>> response = _request(request);

    if (response.first.success()){
        for(const auto &d: response.second){
            std::pair<std::chrono::milliseconds, std::string> v;
            v.first = std::chrono::milliseconds(d.first);
            v.second = d.second;

            result_deque->push_back(v);
        }
    }

    return result_deque;
}

bool sls::Client::append(const std::string &key, const std::string &data){
    assert(key.size() > 0);
    sls::Request request;
    request.set_key(key);

    //TODO: Can do this in one step?
    auto r = request.mutable_req_append();
    r->set_data(data);

    sls::Response retval = _request(request).first;

    return retval.success();
}

bool sls::Client::append(const std::string &key, const std::chrono::milliseconds &time, const std::string &data){
    assert(key.size() > 0);
    sls::Request request;
    request.set_key(key);

    //TODO: Can do this in one step?
    auto r = request.mutable_req_append();
    r->set_data(data);
    r->set_time(time.count());

    sls::Response retval = _request(request).first;

    return retval.success();
}

std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > sls::Client::lastn(const std::string &key, const unsigned long long &num_entries){
    assert(key.size() > 0);

    std::shared_ptr<std::deque<std::pair<std::chrono::milliseconds, std::string>> > result_deque(new std::deque<std::pair<std::chrono::milliseconds, std::string>>);

    sls::Request request;
    request.mutable_last()->set_max_values(num_entries);
    request.set_key(key);

    std::pair<sls::Response, std::vector<std::pair<uint64_t, std::string>>> response = _request(request);

    if (response.first.success()){
        for(const auto &d: response.second){
            std::pair<std::chrono::milliseconds, std::string> v;
            v.first = std::chrono::milliseconds(d.first);
            v.second = d.second;

            result_deque->push_back(v);
        }
    }

    return result_deque;
}

std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > sls::Client::all(const std::string &key){
    assert(key.size() > 0);
    return _interval(key, 0, ULONG_MAX, false);
}

std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > sls::Client::intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end){
    assert(key.size() > 0);
    return _interval(key, start, end, true);
}
