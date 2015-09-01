#include "server.h"
#include <sys/stat.h> //for mkdir()
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include "active_key.h"

namespace sls{

std::shared_ptr<Active_Key> SLS::_get_active_file(const std::string &key, const bool &create_if_missing){
    std::unique_lock<std::mutex> l(_active_file_map_lock);
    try{
        return _active_files.at(key);
    }
    catch(std::out_of_range e){
        if(create_if_missing){
            const auto d = mkdir((_disk_dir + key).c_str(), 0755);
            if( d != 0 ){
                if(errno != EEXIST){
                    throw Fatal_Error();
                }
            }
        }
        std::shared_ptr<Active_Key> active_file(new Active_Key(_disk_dir, key));
        _active_files[key] = active_file;
        return active_file;
    }
}

void SLS::append(const std::string &key, const std::string &data){
    std::shared_ptr<Active_Key> active_key = _get_active_file(key, true);
    active_key->append(data);
}

void SLS::append(const std::string &key, const std::chrono::milliseconds &time, const std::string &data){
    std::shared_ptr<Active_Key> active_key = _get_active_file(key, true);
    active_key->append(data, time);
}

std::string SLS::time_lookup(const std::string &key, const uint64_t &start, const uint64_t &end){
    std::shared_ptr<Active_Key> active_key = _get_active_file(key, false);
    return active_key->time_lookup(start, end);
}

std::string SLS::index_lookup(const std::string &key, const size_t &start, const size_t &end){
    std::shared_ptr<Active_Key> active_key = _get_active_file(key, false);
    return active_key->index_lookup(start, end);
}

std::string SLS::last_lookup(const std::string &key, const size_t &max_values){
    std::shared_ptr<Active_Key> active_key = _get_active_file(key, false);
    return active_key->last_lookup(max_values);
}

SLS::SLS(const std::string &dd){
    _disk_dir = dd;
}

}
