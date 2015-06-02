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

namespace sls{

uint64_t MAX_STACK_ALLOCATION = 1024000;

sls::Value SLS::wrap(const std::string &payload){
    sls::Value r;
    r.set_time(milli_time());
    r.set_data(payload);
    return r;
}

void SLS::_page_out(const std::string &key, const unsigned int &skip){
    syslog(LOG_INFO, "Attempting to page out: %s", key.c_str());
    std::deque<sls::Value>::iterator i = (cache[key]).begin();
    unsigned int j = 0;
    unsigned int size = cache[key].size();
    for(; (j < size) && (j < skip); ++j, ++i);
    std::deque<sls::Value>::iterator new_end = i;

    //pack into archive
    std::unique_ptr<sls::Archive> archive(new sls::Archive);
    for(; i != cache[key].end(); ++i){
        sls::Value *v = archive->add_values();
        //maybe can eliminate this copy and use a pointer instead?
        v->CopyFrom(*i);
    }

    std::string head_link = disk_dir;
    head_link.append(key);
    head_link.append("/head");
    archive->set_next_archive(get_canonical_filename(head_link));

    std::unique_ptr<std::string> outfile(new std::string);
    archive->SerializeToString(outfile.get());

    //write new file
    std::string directory = disk_dir;
    directory.append(key);
    directory.append("/");

    mkdir(directory.c_str(), 0755);
    std::string new_file_name = RandomString(32);
    std::string new_file = directory + "/" + new_file_name;
    if (writefile(new_file, *(outfile.get()))){

    //set symlink from directory/head -> new_file_name
        remove(head_link.c_str());
        symlink(new_file_name.c_str(), head_link.c_str());

        //delete from new_end to end()
        cache[key].erase(new_end, cache[key].end());
    }
    else{
        syslog(LOG_ERR, "Failed to page out: %s", key.c_str());
    }
}

void SLS::_file_lookup(const std::string &key, const std::string &filename, sls::Archive *archive){
    if(filename == ""){
        return;
    }
    std::string filepath = (disk_dir + key + "/" + filename);
    std::unique_ptr<std::string> s(new std::string);
    try{
        readfile(filepath, s.get());
        archive->ParseFromString(*(s.get()));
    }
    catch(...){
        syslog(LOG_ERR, "Could not read from: %s", filepath.c_str());
    }
    return;
}

void SLS::file_lookup(const std::string &key, const std::string &filename, std::deque<sls::Value> *r){
    std::unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());

    r->clear();

    if(archive != nullptr){
        for(int i = 0; i < archive->values_size(); i++){
            r->push_back(archive->values(i));
        }
    }
    return;
}

std::string SLS::next_lookup(const std::string &key, const std::string &filename){
    std::unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());
    std::string next_archive;

    if(archive != nullptr){
        if(archive->has_next_archive()){
            next_archive = archive->next_archive();
        }
    }
    return next_archive;
}

unsigned long long SLS::pick_time(const std::deque<sls::Value> &d, const unsigned long long &start, const unsigned long long &end, std::deque<sls::Value> *result){
    unsigned long long earliest = ULLONG_MAX;
    for(const auto &value: d){
        if( (value.time() > start) && (value.time() < end) ){
            result->push_front(value);
        }
        earliest = std::min(earliest, (unsigned long long)value.time());
    }
    return earliest;
}

unsigned long long SLS::pick(const std::deque<sls::Value> &d, unsigned long long current, const unsigned long long &start, const unsigned long long &end, std::deque<sls::Value> *result){
    for (const auto &value: d){
        if( (current >= start) && (current <= end) ){
            result->push_back(value);
        }
        current++;
    }
    return current;
}

std::deque<sls::Value> SLS::_lookup(const std::string &key, const unsigned long long &start, const unsigned long long &end, const bool &time_lookup){
    std::deque<sls::Value> results;

    std::string next_file;
    std::deque<sls::Value> d;
    {
        std::lock_guard<std::mutex> guard(locks[key]);
        d = cache[key];
        next_file = get_canonical_filename(disk_dir + key + std::string("/head"));
    }


    unsigned long long current = 0;

    bool has_next = true;
    do{
        if( time_lookup ){
            auto earliest_seen = pick_time(d, start, end, &results);
            if( earliest_seen < start ){
                break;
            }
        }
        else{
            current = pick(d, current, start, end, &results);
            if(current >= end ){
                break;
            }
        }
        do{
            file_lookup(key, next_file, &d);
            if(next_file == ""){
                has_next = false;
                break;
            }
            next_file = next_lookup(key, next_file);
            syslog(LOG_DEBUG, "Currently have %zu elements", d.size());
        }
        while(d.size() == 0);
        syslog(LOG_DEBUG, "Currently have %zu elements", d.size());

    }while(has_next);

    return results;
}

std::deque<sls::Value> SLS::time_lookup(const std::string &key, const std::chrono::high_resolution_clock::time_point &start, const std::chrono::high_resolution_clock::time_point &end){
    const auto s = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch());
    const auto e = std::chrono::duration_cast<std::chrono::milliseconds>(end.time_since_epoch());
    return _lookup(key, s.count(), e.count(), true);
}

std::deque<sls::Value> SLS::index_lookup(const std::string &key, const size_t &start, const size_t &end){
    return _lookup(key, start, end, false);
}

void _write_data( const std::string &key, const std::string &data, const std::string &disk_dir){
    const uint64_t current_time = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const uint64_t data_length = data.size();

    std::string head_link = disk_dir;
    head_link.append(key);

    std::ofstream o(head_link, std::ofstream::app | std::ofstream::binary);
    o.write((char *)&current_time, sizeof(uint64_t));
    o.write((char *)&data_length, sizeof(uint64_t));
    o.write(data.c_str(), data_length);
    o.close();

}

std::vector<std::pair<uint64_t, std::string>> _read_file(const std::string &path){
    std::vector<std::pair<uint64_t, std::string>> data;
    std::ifstream i(path, std::ifstream::in | std::ifstream::binary);
    uint64_t timestamp = 0;
    while(i.read((char *)&timestamp, sizeof(uint64_t))){
        uint64_t data_length = 0;
        i.read((char *)&data_length, sizeof(uint64_t));

        char datagram[data_length];
        i.read(datagram, data_length);

        if(i){
            const std::pair<uint64_t, std::string> d(timestamp, std::string(datagram, data_leng
            data.push_back(d);
        }
        else{
            assert(false);
        }
    }
    return data;
}

void SLS::append(const std::string &key, const std::string &data){
    std::unique_lock<std::mutex> l( locks[key] );

    _write_data(key, data, disk_dir);

}

SLS::SLS(const std::string &dd, const unsigned long &min, const unsigned long &max){
    disk_dir = dd;
    cache_min = min;
    cache_max = max;
}

SLS::~SLS(){
    sync();
}

void SLS::sync(){
    for(auto c: cache){
        std::lock_guard<std::mutex> l(locks[c.first]);
        _page_out(c.first, 0);
    }
}

}
