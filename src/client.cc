#include "client.h"
#include "error.h"
#include "sls.h"
#include "archive.h"

#include <smpl.h>
#include <limits.h>

std::shared_ptr<smpl::Remote_Address> sls::global_server = nullptr;

sls::Client::Client(std::shared_ptr<smpl::Remote_Address> server) {
  server_connection = std::unique_ptr<smpl::Channel>(server->connect());
}

std::pair<sls::Response, std::string> sls::Client::_request(const sls::Request &request) {
  assert(request.key().size() > 0);

  std::string request_string;
  request.SerializeToString(&request_string);

  server_connection->send(request_string);
  std::string returned = server_connection->recv();

  sls::Response response;
  response.ParseFromString(returned);

  if (response.data_to_follow()) {
    const auto foo = std::pair<sls::Response, std::string>(response, server_connection->recv());
    return foo;
  }
  else{
    const std::string data;
    return std::pair<sls::Response, std::string>(response, data);
  }
}

std::string sls::Client::_interval(const std::string &key, const unsigned long long &start,
                       const unsigned long long &end, const bool &is_time) {
  assert(key.size() > 0);

  sls::Request request;
  request.mutable_req_range()->set_start(start);
  request.mutable_req_range()->set_end(end);
  request.mutable_req_range()->set_is_time(is_time);
  request.set_key(key);

  const auto response = _request(request);

  if (response.first.success()) {
    return response.second;
  } else {
      const std::string foo;
    return foo;
  }
}

bool sls::Client::append(const std::string &key, const std::string &data) {
  assert(key.size() > 0);
  sls::Request request;
  request.set_key(key);

  auto r = request.mutable_req_append();
  r->set_data(data);

  sls::Response retval = _request(request).first;

  return retval.success();
}

bool sls::Client::append_archive(const std::string &key, const sls::Archive &archive) {
  assert(key.size() > 0);
  sls::Request request;
  request.set_key(key);

  request.set_packed_archive(archive.str());

  sls::Response retval = _request(request).first;

  return retval.success();
}

bool sls::Client::append(const std::string &key,
                         const std::chrono::milliseconds &time,
                         const std::string &data) {
  assert(key.size() > 0);
  sls::Request request;
  request.set_key(key);

  auto r = request.mutable_req_append();
  r->set_data(data);
  r->set_time(time.count());

  sls::Response retval = _request(request).first;

  return retval.success();
}

sls::Archive sls::Client::lastn(const std::string &key,
                   const unsigned long long &num_entries) {
  assert(key.size() > 0);

  sls::Request request;
  request.mutable_last()->set_max_values(num_entries);
  request.set_key(key);

  const auto response = _request(request);

  if (response.first.success()) {
    return sls::Archive(response.second);
  } else {
      throw sls::Request_Failed();
  }
}

sls::Archive sls::Client::all(const std::string &key) {
  assert(key.size() > 0);
  return _interval(key, 0, ULONG_MAX, false);
}

sls::Archive sls::Client::intervalt(const std::string &key, const std::chrono::milliseconds &start,
                       const std::chrono::milliseconds &end) {
  assert(key.size() > 0);
  assert(start <= end);
  return _interval(key, start.count(), end.count(), true);
}

sls::Cached_Client::Cached_Client(std::shared_ptr<smpl::Remote_Address> server, const size_t max_cache_size):
    _client(server){
        _max_cache_size = max_cache_size;
        _current_cache_size = 0;
}

sls::Cached_Client::~Cached_Client(){
    _flush();
}

bool sls::Cached_Client::_flush(){
    bool success = true;
    for(const auto &c: _cache){
        const auto r = _client.append_archive(c.first, c.second);
        success = success && r;
    }
    _cache.clear();
    _current_cache_size = 0;
    return success;
}

bool sls::Cached_Client::_check(){
    if(_current_cache_size > _max_cache_size){
        return _flush();
    }
    return true;
}

bool sls::Cached_Client::_flush_key(const std::string &key){
    const size_t bytes_to_be_flushed = _cache[key].size();
    const auto success = _client.append_archive(key, _cache[key]);

    _cache.erase(key);
    _current_cache_size = _current_cache_size - bytes_to_be_flushed;

    return success;
}

bool sls::Cached_Client::_check_key(const std::string &key){
    if( (_current_cache_size > _max_cache_size) && (_cache[key].size() > (_max_cache_size / _cache.size())) ){
        return _flush_key(key);
    }
    else{
        return true;
    }
}

bool sls::Cached_Client::flush(){
    return _flush();
}

bool sls::Cached_Client::append(const std::string &key, const std::string &data){
    const std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
    return append(key, timestamp, data);
}

bool sls::Cached_Client::append(const std::string &key, const std::chrono::milliseconds &time,
            const std::string &data){
    try{
        size_t bytes_appended = _cache[key].append(time, data);
        _current_cache_size = _current_cache_size + bytes_appended;
    }
    catch(Out_Of_Order e){
        return false;
    }
    return _check_key(key);
}

bool sls::Cached_Client::append_archive(const std::string &key, const sls::Archive &archive){
    try{
        size_t bytes_appended = _cache[key].append(archive);
        _current_cache_size = _current_cache_size + bytes_appended;
    }
    catch(Out_Of_Order e){
        return false;
    }
    return _check_key(key);
}

sls::Archive sls::Cached_Client::lastn(const std::string &key, const unsigned long long &num_entries){
return _client.lastn(key, num_entries);
}

sls::Archive sls::Cached_Client::all(const std::string &key){
return _client.all(key);
}

sls::Archive sls::Cached_Client::intervalt(const std::string &key, const std::chrono::milliseconds &start,
        const std::chrono::milliseconds &end){
return _client.intervalt(key, start, end);
}
