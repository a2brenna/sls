#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <algorithm>

#include "hgutil/files.h"
#include "hgutil/strings.h"
#include "hgutil/time.h"
#include "sls.h"
#include "server.h"
#include "archive.h"
#include "file.h"
#include "active_key.h"

#include <fstream>

namespace sls{

std::shared_ptr<Active_Key> SLS::_get_active_file(const std::string &key){
    std::unique_lock<std::mutex> l(_active_file_map_lock);
    try{
        return _active_files.at(key);
    }
    catch(std::out_of_range e){
        //TODO: move this? will cause creation of empty directories here...
        const auto d = mkdir((disk_dir + key).c_str(), 0755);
        if( d != 0 ){
            if(errno != EISDIR){
                throw Fatal_Error();
            }
        }
        std::shared_ptr<Active_Key> active_file(new Active_Key(disk_dir, key, 0));
        _active_files[key] = active_file;
        return active_file;
    }
}

void SLS::append(const std::string &key, const std::string &data){
    std::shared_ptr<Active_Key> backend_file = _get_active_file(key);
    backend_file->append(data);
}

std::string SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    std::unique_lock<std::mutex> coarse_lock( maps_lock );
    std::unique_lock<std::mutex> fine_lock( disk_locks[key] );
    coarse_lock.unlock();

    std::string result;

    const uint64_t start_time = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    const uint64_t end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()).count();

    Path index_path(disk_dir + key + "/index");
    Index index(index_path);
    std::vector<Index_Record> files = index.time_lookup(start, end);
    if(files.empty()){
        return result;
    }
    else{
        for(const auto &f: files){
            Path path(disk_dir + key + "/" + f.filename());
            Archive arch(path);
            while(true){
                try{
                    const uint64_t current_time = arch.head_time();

                    if( current_time < start_time ){
                        arch.advance_index();
                    }
                    else if( current_time > end_time){
                        break;
                    }
                    else if( current_time >= start_time && current_time <= end_time){
                        result.append(arch.head_record());
                        arch.advance_index();
                    }
                    else{
                        assert(false);
                    }
                }
                catch(End_Of_Archive e){
                    break;
                }
            }
        }
        return result;
    }
}

std::string SLS::index_lookup(const std::string &key, const size_t &start, const size_t &end){
    std::unique_lock<std::mutex> coarse_lock( maps_lock );
    std::unique_lock<std::mutex> fine_lock( disk_locks[key] );
    coarse_lock.unlock();

    std::string result;

    Path index_path(disk_dir + key + "/index");
    Index index(index_path);
    std::vector<Index_Record> files = index.position_lookup(start, end);
    if(files.empty()){
        return result;
    }
    else{
        for(const auto &f: files){
            size_t current_index = f.position();
            Path path(disk_dir + key + "/" + f.filename());
            Archive arch(path);
            while(true){
                try{
                    if( current_index < start){
                        arch.advance_index();
                        current_index++;
                    }
                    else if( current_index > end){
                        break;
                    }
                    else if( current_index >= start && current_index <= end){
                        result.append(arch.head_record());
                        arch.advance_index();
                        current_index++;
                    }
                    else{
                        assert(false);
                    }
                }
                catch(End_Of_Archive e){
                    break;
                }
            }
        }
        return result;
    }
}

std::string SLS::last_lookup(const std::string &key, const size_t &max_values){
    //TODO: Implement efficient lookups here
    std::string result;
    return result;
}

SLS::SLS(const std::string &dd){
    disk_dir = dd;
}

SLS::~SLS(){
}

}
