#include "server.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <hgutil/files.h>
#include "active_key.h"

namespace sls {

std::shared_ptr<Active_Key>
SLS::_get_active_file(const std::string &key, const bool &create_if_missing) {
  std::unique_lock<std::mutex> l(_active_file_map_lock);
  try {
    return _active_files.at(key);
  } catch (std::out_of_range e) {
      std::shared_ptr<Active_Key> active_file(new Active_Key(_disk_dir, key, create_if_missing));
    _active_files[key] = active_file;
    return active_file;
  }
}

void SLS::append(const std::string &key, const std::string &data) {
  std::shared_ptr<Active_Key> active_key = _get_active_file(key, true);
  active_key->append(data);
}

void SLS::append(const std::string &key, const std::chrono::milliseconds &time,
                 const std::string &data) {
  std::shared_ptr<Active_Key> active_key = _get_active_file(key, true);
  active_key->append(data, time);
}

void SLS::append_archive(const std::string &key, const sls::Archive &archive) {
  std::shared_ptr<Active_Key> active_key = _get_active_file(key, true);
  active_key->append_archive(archive);
}

sls::Archive SLS::time_lookup(const std::string &key,
                             const std::chrono::milliseconds &start,
                             const std::chrono::milliseconds &end) {
  std::shared_ptr<Active_Key> active_key = _get_active_file(key, false);
  return active_key->time_lookup(start, end);
}

sls::Archive SLS::index_lookup(const std::string &key, const size_t &start,
                              const size_t &end) {
  std::shared_ptr<Active_Key> active_key = _get_active_file(key, false);
  return active_key->index_lookup(start, end);
}

sls::Archive SLS::last_lookup(const std::string &key, const size_t &max_values) {
  std::shared_ptr<Active_Key> active_key = _get_active_file(key, false);
  return active_key->last_lookup(max_values);
}

std::vector<std::string> SLS::get_all_keys() const{
    std::vector<std::string> keys;
    std::vector<std::string> buckets;
    getdir(_disk_dir, buckets);
    for(const auto &b: buckets){
        std::vector<std::string> keys_in_bucket;
        getdir(_disk_dir + "/" + b, keys_in_bucket);
        keys.insert(keys.end(), keys_in_bucket.begin(), keys_in_bucket.end());
    }
    return keys;
}

SLS::SLS(const std::string &dd) { _disk_dir = dd; }
}
