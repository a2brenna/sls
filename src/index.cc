#include "index.h"
#include <hgutil/files.h>
#include <sstream>
#include <fstream>
#include <cassert>

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

std::istream& operator>>(std::istream& in, Index &i){
    while(in){
        Index_Record r;
        in >> r;
        i.append(r);
    }

    return in;
}

void Index::append(const Index_Record &r){
    _index.push_back(r);
}

Index build_index(const Path &directory){
    Index index;
    std::vector<std::string> files;
    const auto m = getdir(directory.str(), files);
    assert(m == 0);

    uint64_t position = 0;

    for(const auto &file: files){
        std::ifstream i(directory.str() + "/" + file, std::ofstream::binary);
        assert(i);

        //have to iterate over file entries so we can get position of start and end elements
        uint64_t first_timestamp;
        i.read((char *)&first_timestamp, sizeof(uint64_t));
        uint64_t first_position = position;

        uint64_t temp_timestamp;
        uint64_t payload_size;
        do{
            position++;
            i.read((char *)&payload_size, sizeof(uint64_t));
            i.seekg(payload_size, std::ios::cur);
        }
        while(i.read((char *)&temp_timestamp, sizeof(uint64_t)));


        index.append(Index_Record(first_timestamp, first_position, file, 0));
    }

    return index;
}
