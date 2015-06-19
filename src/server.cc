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
#include "file.h"

#include <fstream>

namespace sls{

std::deque<std::pair<uint64_t, std::string>> _read_file(const std::string &path){
    std::deque<std::pair<uint64_t, std::string>> data;

    std::ifstream i(path, std::ifstream::in | std::ifstream::binary);
    assert(i);

    uint64_t timestamp = 0;
    while(i.read((char *)&timestamp, sizeof(uint64_t))){
        uint64_t data_length = 0;
        i.read((char *)&data_length, sizeof(uint64_t));

        char datagram[data_length];
        i.read(datagram, data_length);

        if(i){
            const std::pair<uint64_t, std::string> d(timestamp, std::string(datagram, data_length));
            data.push_back(d);
        }
        else{
            assert(false);
        }
    }

    return data;
}

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
        fine_lock = std::unique_lock<std::mutex>(write_locks[key]);
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

std::deque<std::pair<uint64_t, std::string>> SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    assert(start < end);

    std::deque<std::pair<uint64_t, std::string>> result;

    const auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch());
    const auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch());

    std::vector<Index_Record> files = _index_time_lookup(key, start, end);

    for(const auto &f: files){
        std::unique_lock<std::mutex> coarse_lock( maps_lock );
        std::unique_lock<std::mutex> fine_lock( write_locks[key] );
        coarse_lock.unlock();
        const std::string path = disk_dir + key + "/" + f.filename();
        const std::deque<std::pair<uint64_t, std::string>> dataset = _read_file(path);
        fine_lock.unlock();

        for(const auto &element: dataset){
            std::chrono::milliseconds timestamp(element.first);
            if( (timestamp > start_time) && (timestamp < end_time) ){
                result.push_back(std::pair<uint64_t, std::string>(element.first, element.second));
            }
        }
    }

    return result;
}

std::deque<std::pair<uint64_t, std::string>> SLS::index_lookup(const std::string &key, const size_t &start, const size_t &end){
    assert(start > 0);
    assert(start < end);

    std::deque<std::pair<uint64_t, std::string>> result;

    std::vector<Index_Record> files = _index_position_lookup(key, start, end);

    if(files.empty()){
        return result;
    }
    else{
        uint64_t current_position = files.front().position();

        for(const auto &f: files){
            std::unique_lock<std::mutex> coarse_lock( maps_lock );
            std::unique_lock<std::mutex> fine_lock( write_locks[key] );
            coarse_lock.unlock();
            const std::string path = disk_dir + key + "/" + f.filename();
            const std::deque<std::pair<uint64_t, std::string>> dataset = _read_file(path);
            fine_lock.unlock();

            //TODO: skip to start based on numeric position of first entry in first file if possible
            for(size_t i = 0; i < dataset.size(); i++){
                if( current_position < start ){
                    //do nothing but jump to bottom and increment
                }
                else if( current_position <= end ){
                    result.push_back(std::pair<uint64_t, std::string>(dataset[i].first, dataset[i].second));
                }
                else if( current_position > end){
                    return result;
                }
                else{
                    assert(false);
                }
                current_position++;
            }
        }

        return result;
    }
}

std::string SLS::raw_time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
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
            std::unique_lock<std::mutex> coarse_lock( maps_lock );
            std::unique_lock<std::mutex> fine_lock( write_locks[key] );
            coarse_lock.unlock();
            {
                const std::string path = disk_dir + key + "/" + f.filename();
                std::ifstream i(path, std::ifstream::in | std::ifstream::binary);
                assert(i);

                uint64_t timestamp = 0;
                while(i.read((char *)&timestamp, sizeof(uint64_t))){
                    if(timestamp > start_time){
                        break;
                    }
                    else{
                        uint64_t data_length = 0;
                        i.read((char *)&data_length, sizeof(uint64_t));
                        i.seekg(data_length, std::ios::cur);
                    }
                }

                do{
                    if(timestamp > end_time){
                        return result;
                    }
                    else{
                        result.append(std::string( (char *)(&timestamp), sizeof(uint64_t)));

                        uint64_t data_length = 0;
                        i.read((char *)&data_length, sizeof(uint64_t));

                        result.append(std::string( (char *)(&data_length), sizeof(uint64_t)));

                        char datagram[data_length];
                        i.read(datagram, data_length);

                        if(i){
                            result.append(std::string( datagram, data_length ));
                        }
                        else{
                            assert(false);
                        }
                    }
                } while(i.read((char *)&timestamp, sizeof(uint64_t)));
            }
        }

        return result;
    }
}

std::string SLS::raw_index_lookup(const std::string &key, const size_t &start, const size_t &end){
    assert(start > 0);
    assert(start < end);

    std::string result;

    std::vector<Index_Record> files = _index_position_lookup(key, start, end);
    if(files.empty()){
        return result;
    }
    else{
        size_t index = files.front().position();

        for(const auto &f: files){
            std::unique_lock<std::mutex> coarse_lock( maps_lock );
            std::unique_lock<std::mutex> fine_lock( write_locks[key] );
            coarse_lock.unlock();
            {
                const std::string path = disk_dir + key + "/" + f.filename();
                std::ifstream i(path, std::ifstream::in | std::ifstream::binary);
                assert(i);

                uint64_t timestamp = 0;
                while(i.read((char *)&timestamp, sizeof(uint64_t))){
                    if(index >= start){
                        break;
                    }
                    else{
                        index++;
                        uint64_t data_length = 0;
                        i.read((char *)&data_length, sizeof(uint64_t));
                        i.seekg(data_length, std::ios::cur);
                    }
                }

                do{
                    if(index > end){
                        return result;
                    }
                    else{
                        result.append(std::string( (char *)(&timestamp), sizeof(uint64_t)));

                        uint64_t data_length = 0;
                        i.read((char *)&data_length, sizeof(uint64_t));

                        result.append(std::string( (char *)(&data_length), sizeof(uint64_t)));

                        char datagram[data_length];
                        i.read(datagram, data_length);

                        if(i){
                            result.append(std::string( datagram, data_length ));
                        }
                        else{
                            assert(false);
                        }
                    }
                    index++;
                } while(i.read((char *)&timestamp, sizeof(uint64_t)));
            }
        }

        return result;
    }
}

SLS::SLS(const std::string &dd){
    disk_dir = dd;
}

SLS::~SLS(){
}

}
