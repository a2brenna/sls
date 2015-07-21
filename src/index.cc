#include "index.h"
#include <hgutil/files.h>
#include <sstream>
#include <fstream>
#include <cassert>
#include <algorithm>
#include "archive.h"

Index_Record::Index_Record(const uint64_t &timestamp, const uint64_t &position, const std::string &filename, const uint64_t &offset){
    _timestamp = timestamp;
    _position = position;
    _filename = filename;
    _offset = offset;
}

Index_Record::Index_Record(){
}

uint64_t Index_Record::timestamp() const{
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
    out << i.timestamp() << " ";
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

    Index_Record r(timestamp, position, filename, offset);
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

const std::vector<Index_Record> Index::time_lookup(const uint64_t &start, const uint64_t &end){
    assert(start <= end);
    std::vector<Index_Record> files;

    if(_index.empty()){
        return files;
    }
    else{
        assert(_index.size() > 0);

        size_t is = 0;
        size_t ie = _index.size() - 1;
        size_t i = 0;
        for( ; i < _index.size(); i++){
            if(_index[i].timestamp() < start){
                is = i;
            }
        }
        for(; i < _index.size(); i++){
            if(_index[i].timestamp() < end){
                ie = i;
            }
        }

        assert( is <= ie);
        assert( ie < _index.size() );

        for(size_t i = is; i <= ie; i++){
            files.push_back(_index[i]);
        }

        return files;
    }
}

const std::vector<Index_Record> Index::position_lookup(const uint64_t &start, const uint64_t &end){
    assert(start >= 0);
    assert(start <= end);
    std::vector<Index_Record> files;

    if(_index.empty()){
        return files;
    }
    else{
        assert(_index.size() > 0);

        size_t is = 0;
        size_t ie = _index.size() - 1;

        size_t i = 0;
        for( ; i < _index.size(); i++){
            if(_index[i].position() < start){
                is = i;
            }
        }
        for(; i < _index.size(); i++){
            if(_index[i].position() < end){
                ie = i;
            }
        }

        assert( is <= ie);
        assert( ie < _index.size() );

        for(size_t i = is; i <= ie; i++){
            files.push_back(_index[i]);
        }

        return files;
    }
}

size_t Index::num_elements() const{
    if(_index.empty()){
        return 0;
    }
    else{
        return _index.back().position();
    }
}

Index build_index(const Path &directory){
    std::vector<std::pair<uint64_t, Index_Record>> timestamp_records;
    std::vector<std::string> files;
    const auto m = getdir(directory.str(), files);
    assert(m == 0);

    uint64_t position = 0;

    for(const auto &file: files){
        Path arch_path(directory.str() + "/" + file);
        Archive arch(arch_path);

        const uint64_t first_timestamp = arch.head_time();
        const uint64_t first_position = position;

        uint64_t last_timestamp = first_timestamp;
        uint64_t last_index = 0;

        for(;;){
            try{
                arch.advance_index();
                position++;
                last_timestamp = arch.head_time();
                last_index = arch.index();
            }
            catch(End_Of_Archive e){
                break;
            }
        }

        timestamp_records.push_back(std::pair<uint64_t, Index_Record>(first_timestamp, Index_Record(first_timestamp, first_position, file, 0)));
        timestamp_records.push_back(std::pair<uint64_t, Index_Record>(last_timestamp, Index_Record(last_timestamp, position, file, last_index)));
    }

    std::sort(timestamp_records.begin(), timestamp_records.end());

    Index index;
    for(const auto &r: timestamp_records){
        index.append(r.second);
    }
    return index;
}

