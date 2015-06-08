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
#include <stack>
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

std::vector<std::pair<uint64_t, std::string>> _read_file(const std::string &path){
    std::vector<std::pair<uint64_t, std::string>> data;

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
    std::unique_lock<std::mutex> l( locks[key] );
    _write_data(key, data, disk_dir);
}

std::deque<sls::Value> SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    assert(start < end);

    const auto dataset = _read_file(disk_dir + key);
    const auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch());
    const auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch());

    std::deque<sls::Value> result;

    for(const auto &element: dataset){
        std::chrono::milliseconds timestamp(element.first);
        if( (timestamp > start_time) && (timestamp < end_time) ){
            sls::Value v;
            v.set_time(element.first);
            v.set_data(element.second);

            result.push_back(v);
        }
    }

    return result;
}

std::deque<sls::Value> SLS::index_lookup(const std::string &key, const size_t &start, const size_t &end){
    assert(start > 0);
    assert(start < end);

    std::deque<sls::Value> result;

    const auto dataset = _read_file(disk_dir + key);
    size_t i = start - 1;
    size_t e = std::min(dataset.size(), end);

    for(; i < e; i++){
        sls::Value v;
        v.set_time(dataset[i].first);
        v.set_data(dataset[i].second);

        result.push_back(v);
    }

    return result;

}

SLS::SLS(const std::string &dd){
    disk_dir = dd;
}

SLS::~SLS(){
}

}
