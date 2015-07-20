#include "server.h"
#include <sys/stat.h> //for mkdir()
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include "hgutil/files.h"
#include "hgutil/time.h"
#include "active_key.h"

namespace sls{

std::shared_ptr<Active_Key> SLS::_get_active_file(const std::string &key){
    std::unique_lock<std::mutex> l(_active_file_map_lock);
    try{
        return _active_files.at(key);
    }
    catch(std::out_of_range e){
        //TODO: move this? will cause creation of empty directories here...
        const auto d = mkdir((_disk_dir + key).c_str(), 0755);
        if( d != 0 ){
            if(errno != EISDIR){
                throw Fatal_Error();
            }
        }
        std::shared_ptr<Active_Key> active_file(new Active_Key(_disk_dir, key));
        _active_files[key] = active_file;
        return active_file;
    }
}

void SLS::append(const std::string &key, const std::string &data){
    std::shared_ptr<Active_Key> backend_file = _get_active_file(key);
    backend_file->append(data);
}

std::string SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    std::shared_ptr<Active_Key> backend_file = _get_active_file(key);
    return backend_file->time_lookup(start, end);
}

std::string SLS::index_lookup(const std::string &key, const size_t &start, const size_t &end){
    std::shared_ptr<Active_Key> backend_file = _get_active_file(key);
    return backend_file->index_lookup(start, end);
}

std::string SLS::last_lookup(const std::string &key, const size_t &max_values){
    std::shared_ptr<Active_Key> backend_file = _get_active_file(key);
    return backend_file->last_lookup(max_values);
}

SLS::SLS(const std::string &dd){
    _disk_dir = dd;
}

SLS::~SLS(){
}

}
