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

    std::vector<std::string> files;
    const auto m = getdir(directory, files);
    assert(m == 0);

    for(const auto &file: files){
        std::ifstream i(directory + "/" + file, std::ofstream::binary);
        assert(i);

        uint64_t timestamp;
        assert(i.read((char *)&timestamp, sizeof(uint64_t)));

        index.push_back(Index_Record(timestamp, file, 0));
    }

    return index;
}
