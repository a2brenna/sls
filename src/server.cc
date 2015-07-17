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

#include <fstream>

namespace sls{

std::vector<Index_Record> SLS::_index_time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    std::vector<Index_Record> files;
    std::unique_lock<std::mutex> l( maps_lock );

    std::string index_file_path = disk_dir + key + "/index";

    std::ifstream infile(index_file_path, std::ofstream::in);
    if(!infile){
        return files;
    }

    Index index;
    infile >> index;

    const uint64_t s = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    const uint64_t e = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()).count();

    if(index.index().empty()){
        return files;
    }
    else{
        assert(index.index().size() > 0);

        size_t is = 0;
        size_t ie = index.index().size() - 1;

        size_t i = 0;
        for( ; i < index.index().size(); i++){
            if(index.index()[i].timestamp() < s){
                is = i;
            }
        }
        for(; i < index.index().size(); i++){
            if(index.index()[i].timestamp() < e){
                ie = i;
            }
        }

        assert( is <= ie);
        assert( ie < index.index().size() );

        for(size_t i = is; i <= ie; i++){
            files.push_back(index.index()[i]);
        }

        return files;
    }
}

std::vector<Index_Record> SLS::_index_position_lookup(const std::string &key, const uint64_t &start, const uint64_t &end){
    std::vector<Index_Record> files;
    std::unique_lock<std::mutex> l( maps_lock );

    std::string index_file_path = disk_dir + key + "/index";
    std::ifstream infile(index_file_path, std::ofstream::in);
    if(!infile){
        return files;
    }

    Index index;
    infile >> index;

    if(index.index().empty()){
        return files;
    }
    else{
        assert(index.index().size() > 0);

        size_t is = 0;
        size_t ie = index.index().size() - 1;

        size_t i = 0;
        for( ; i < index.index().size(); i++){
            if(index.index()[i].position() < start){
                is = i;
            }
        }
        for(; i < index.index().size(); i++){
            if(index.index()[i].position() < end){
                ie = i;
            }
        }

        assert( is <= ie);
        assert( ie < index.index().size() );

        for(size_t i = is; i <= ie; i++){
            files.push_back(index.index()[i]);
        }

        return files;
    }
}

void SLS::append(const std::string &key, const std::string &data){
    const uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    std::string backend_file;

    std::unique_lock<std::mutex> fine_lock;
    {
        std::unique_lock<std::mutex> coarse_lock( maps_lock );
        fine_lock = std::unique_lock<std::mutex>(disk_locks[key]);
        try{
            backend_file = active_files.at(key);
        }
        catch(std::out_of_range e){
            std::string dir = disk_dir + key + "/";
            backend_file = dir + RandomString(32);
            active_files[key] = backend_file;
            const auto d = mkdir(dir.c_str(), 0755);
            if( d != 0 ){
                if(errno != EISDIR){
                    return;
                }
            }
        }
    }

    assert(backend_file.size() > 0);

    const uint64_t data_length = data.size();

    std::ofstream o(backend_file, std::ofstream::app | std::ofstream::binary);
    assert(o);

    o.write((char *)&current_time, sizeof(uint64_t));
    o.write((char *)&data_length, sizeof(uint64_t));
    o.write(data.c_str(), data_length);

    o.close();
}

std::string SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    std::unique_lock<std::mutex> coarse_lock( maps_lock );
    std::unique_lock<std::mutex> fine_lock( disk_locks[key] );
    coarse_lock.unlock();

    assert(start < end);

    std::string result;

    const uint64_t start_time = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    const uint64_t end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()).count();

    std::vector<Index_Record> files = _index_time_lookup(key, start, end);
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
    assert(start > 0);
    assert(start < end);

    std::unique_lock<std::mutex> coarse_lock( maps_lock );
    std::unique_lock<std::mutex> fine_lock( disk_locks[key] );
    coarse_lock.unlock();

    std::string result;

    std::vector<Index_Record> files = _index_position_lookup(key, start, end);
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
