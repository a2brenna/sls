#include "index.h"
#include <sstream>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <map>
#include "archive.h"
#include "file.h"

#include <iostream>

Index_Record::Index_Record(const std::chrono::milliseconds &timestamp, const uint64_t &position, const std::string &filename, const uint64_t &offset){
    _timestamp = timestamp;
    _position = position;
    _filename = filename;
    _offset = offset;
}

Index_Record::Index_Record(){
}

std::chrono::milliseconds Index_Record::timestamp() const{
    return _timestamp;
}

std::string Index_Record::filename() const{
    return _filename;
}

uint64_t Index_Record::offset() const{
    return _offset;
}

uint64_t Index_Record::position() const{
    return _position;
}

bool operator<(const Index_Record &rhs, const Index_Record &lhs){
    return (rhs.timestamp() < lhs.timestamp()) && (rhs.position() < lhs.position());
}

bool operator>(const Index_Record &rhs, const Index_Record &lhs){
    return (rhs.timestamp() > lhs.timestamp()) && (rhs.position() > lhs.position());
}

const std::vector<Index_Record> &Index::index() const{
    return _index;
}

std::ostream& operator<<(std::ostream& out, const Index_Record &i){
    out << i.timestamp().count() << " ";
    out << i.position() << " ";
    out << i.filename() << " ";
    out << i.offset() << std::endl;
    return out;
}

std::istream& operator>>(std::istream& in, Index_Record &i){
    uint64_t timestamp;
    in >> timestamp;
    uint64_t position;
    in >> position;
    std::string filename;
    in >> filename;
    uint64_t offset;
    in >> offset;

    Index_Record r(std::chrono::milliseconds(timestamp), position, filename, offset);
    i = r;

    return in;
}

std::ostream& operator<<(std::ostream& out, const Index &i){
    for(const Index_Record &r: i.index()){
        out << r;
    }

    return out;
}

Index::Index(){
}

Index::Index(const Path &file){
    std::ifstream infile(file.str(), std::ofstream::in);
    while(infile){
        Index_Record r;
        infile >> r;
        _index.push_back(r);
    }
    //TODO: FIX THIS
    //This alwasy fetches an additional bad record
    if(_index.size() > 1){
        _index.pop_back();
    }
}

void Index::append(const Index_Record &r){
    if (_index.empty()){
        _index.push_back(r);
    }
    else{
        if( (r.position() > _index.back().position()) && (r.timestamp() >= _index.back().timestamp()) ){
            _index.push_back(r);
        }
        else{
            throw Out_of_Order();
        }
    }
}

const std::vector<Index_Record> Index::get_records(const std::string &chunk_name) const{
    std::vector<Index_Record> records;

    for(const auto &r: _index){
        if(r.filename() == chunk_name){
            records.push_back(r);
        }
    }

    return records;
}

const std::vector<Index_Record> Index::time_lookup(const std::chrono::milliseconds &start, const std::chrono::milliseconds &end){
    assert(start <= end);
    std::vector<Index_Record> files;

    std::string last_file = "";

    if(_index.empty()){
        return files;
    }
    else{
        assert(_index.size() > 0);

        size_t is = 0;
        size_t ie = _index.size() - 1;
        size_t i = 0;
        for( ; i < _index.size(); i++){
            if(_index[i].timestamp() <= start){
                is = i;
            }
        }
        for(; i < _index.size(); i++){
            if(_index[i].timestamp() <= end){
                ie = i;
            }
        }

        assert( is <= ie);
        assert( ie < _index.size() );

        for(size_t i = is; i <= ie; i++){
            if( _index[i].filename() !=  last_file ){
                files.push_back(_index[i]);
            }
            last_file = _index[i].filename();
        }

        return files;
    }
}

const std::vector<Index_Record> Index::position_lookup(const uint64_t &start, const uint64_t &end){
    assert(start >= 0);
    assert(start <= end);
    std::vector<Index_Record> records;

    std::string last_file = "";

    if(_index.empty()){
        return records;
    }
    else{
        assert(_index.size() > 0);

        size_t is = 0;
        size_t ie = _index.size() - 1;

        size_t i = 0;
        for( ; i < _index.size(); i++){
            if(_index[i].position() <= start){
                is = i;
            }
        }
        for(; i < _index.size(); i++){
            if(_index[i].position() <= end){
                ie = i;
            }
        }

        assert( is <= ie);
        assert( ie < _index.size() );

        for(size_t i = is; i <= ie; i++){
            if( _index[i].filename() !=  last_file ){
                records.push_back(_index[i]);
            }
            last_file = _index[i].filename();
        }

        return records;
    }
}

size_t Index::num_elements() const{
    if(_index.empty()){
        return 0;
    }
    else{
        return _index.back().position() + 1; //Index positions counts from 0
    }
}

Index build_index(const Path &directory, const size_t &resolution){
    std::vector<std::pair<std::chrono::milliseconds, Index_Record>> timestamp_records;
    std::vector<std::string> files;
    const auto m = getdir(directory.str(), files);
    assert(m == 0);

    std::map<std::string, size_t> size_map;

    for(const auto &file: files){
        bool dirty = false;
        if(file == "index"){
            continue;
        }
        Path arch_path(directory.str() + "/" + file);
        Archive arch(arch_path);

        const std::chrono::milliseconds first_timestamp = arch.head_time();
        timestamp_records.push_back(std::pair<std::chrono::milliseconds, Index_Record>(first_timestamp, Index_Record(first_timestamp, 0, file, 0)));

        size_t count = 1;
        uint64_t last_index = 0;

        std::chrono::milliseconds last_timestamp = first_timestamp;

        for(;;){
            try{
                if( (count % resolution == 0) && dirty ){
                    timestamp_records.push_back(std::pair<std::chrono::milliseconds, Index_Record>(last_timestamp, Index_Record(last_timestamp, count - 1, file, last_index)));
                    dirty = false;
                }
                arch.advance_index();
                last_timestamp = arch.head_time();
                last_index = arch.index();
                count++;
                dirty = true;
            }
            catch(End_Of_Archive e){
                break;
            }
        }

        assert(count > 0);
        assert(last_timestamp >= first_timestamp);
        assert(last_index > 0);

        if(dirty){
            timestamp_records.push_back(std::pair<std::chrono::milliseconds, Index_Record>(last_timestamp, Index_Record(last_timestamp, count - 1, file, last_index)));
        }
        size_map[file] = count;
    }

    std::sort(timestamp_records.begin(), timestamp_records.end());

    Index index;
    size_t count = 0;
    std::string previous_filename = "";
    for(const auto &r: timestamp_records){
        if(r.second.filename() != previous_filename){
            count = count + size_map[previous_filename];
        }
        try{
            index.append(Index_Record(r.second.timestamp(), count + r.second.position(), r.second.filename(), r.second.offset()));
        }
        catch(Out_of_Order o){
            std::cerr << directory.str() << "/" + r.second.filename() << " contains out of order data " << std::endl;
        }
        previous_filename = r.second.filename();
    }
    return index;
}
