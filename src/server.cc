#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "hgutil/files.h"
#include "hgutil/strings.h"
#include "hgutil/time.h"
#include "sls.h"
#include "server.h"
#include "sls.pb.h"
#include <smpl.h>

#include <fstream>

#include <iostream>

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

void _write_data( const std::string &key, const std::string &data, const std::string &disk_dir){
    const uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    const uint64_t data_length = data.size();

    std::string head_link = disk_dir + key;

    std::ofstream o(head_link, std::ofstream::app | std::ofstream::binary);

    o.write((char *)&current_time, sizeof(uint64_t));
    o.write((char *)&data_length, sizeof(uint64_t));
    o.write(data.c_str(), data_length);
    o.close();

}

void SLS::append(const std::string &key, const std::string &data){
    std::unique_lock<std::mutex> fine_lock;
    {
        std::unique_lock<std::mutex> coarse_lock( locks_lock );
        fine_lock = std::unique_lock<std::mutex>(locks[key]);
    }
    _write_data(key, data, disk_dir);
}

std::deque<std::pair<uint64_t, std::string>> SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    assert(start < end);

    std::deque<std::pair<uint64_t, std::string>> result;

    const auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch());
    const auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch());

    //TODO: Fix this to use index to get file list
    std::vector<std::string> files;
    files.push_back(disk_dir + key);

    for(const auto &f: files){
        std::unique_lock<std::mutex> coarse_lock( locks_lock );
        std::unique_lock<std::mutex> fine_lock( locks[key] );
        coarse_lock.unlock();
        const std::deque<std::pair<uint64_t, std::string>> dataset = _read_file(f);
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

    //TODO: Fix this to use index to get file list
    std::vector<std::string> files;
    files.push_back(disk_dir + key);

    uint64_t current_position = 0; //TODO: set this to the numeric position of the first entry in the first file in the list

    for(const auto &f: files){
        std::unique_lock<std::mutex> coarse_lock( locks_lock );
        std::unique_lock<std::mutex> fine_lock( locks[key] );
        coarse_lock.unlock();
        const std::deque<std::pair<uint64_t, std::string>> dataset = _read_file(f);
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

std::string SLS::raw_time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    assert(start < end);

    std::string result;

    const uint64_t start_time = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    const uint64_t end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch()).count();

    //TODO: Fix this to use index to get file list
    std::vector<std::string> files;
    files.push_back(disk_dir + key);

    for(const auto &f: files){
        std::unique_lock<std::mutex> coarse_lock( locks_lock );
        std::unique_lock<std::mutex> fine_lock( locks[key] );
        coarse_lock.unlock();
        {
            const std::string path = disk_dir + key;
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

std::string SLS::raw_index_lookup(const std::string &key, const size_t &start, const size_t &end){
    assert(start > 0);
    assert(start < end);

    std::string result;

    //TODO: Fix this to use index to get file list
    std::vector<std::string> files;
    files.push_back(disk_dir + key);
    size_t index = 0; //TODO: set to numeric position of first entry in first file by index

    for(const auto &f: files){
        std::unique_lock<std::mutex> coarse_lock( locks_lock );
        std::unique_lock<std::mutex> fine_lock( locks[key] );
        coarse_lock.unlock();
        {
            const std::string path = disk_dir + key;
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

SLS::SLS(const std::string &dd){
    disk_dir = dd;
}

SLS::~SLS(){
}

}
