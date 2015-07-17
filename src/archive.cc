#include "archive.h"
#include <hgutil/files.h>
#include <cassert>

Archive::Archive(const Path &file){
    _raw = readfile(file.str());
    _index = 0;
}

Archive::Archive(const std::string &raw){
    _raw = raw;
    _index = 0;
}

uint64_t Archive::head_time() const{
    const char *i = _raw.c_str() + _index;
    if(i == (_raw.c_str() + _raw.size())){
        throw End_Of_Archive();
    }
    assert( i < (_raw.c_str() + _raw.size()) );

    const uint64_t timestamp = *( (uint64_t *)(i) );
    return timestamp;
}

std::string Archive::head_data() const{
    const char *i = _raw.c_str() + _index;
    if(i == (_raw.c_str() + _raw.size())){
        throw End_Of_Archive();
    }
    assert( i < (_raw.c_str() + _raw.size()) );

    const uint64_t blob_length = *( (uint64_t *)(i + sizeof(uint64_t)) );
    const char *blob_start = i + sizeof(uint64_t)*2;

    return std::string( blob_start, blob_length);
}

std::vector<std::pair<uint64_t, std::string>> Archive::extract() const{
    const char *i = _raw.c_str() + _index;
    if(i == (_raw.c_str() + _raw.size())){
        throw End_Of_Archive();
    }
    assert( i < (_raw.c_str() + _raw.size()) );

    std::vector<std::pair<uint64_t, std::string>> output;

    const char *end = _raw.c_str() + _raw.size();
    assert( *end == '\0' );

    while( i < end ){
        uint64_t timestamp;
        timestamp = *( (uint64_t *)(i) );
        i += sizeof(uint64_t);

        uint64_t blob_length;
        blob_length = *( (uint64_t *)(i) );
        i += sizeof(uint64_t);

        output.push_back(std::pair<uint64_t, std::string>(timestamp, std::string(i, blob_length)));
        i += blob_length;
    }

    return output;
}

void Archive::advance_index(){
    const char *i = _raw.c_str() + _index;
    if(i == (_raw.c_str() + _raw.size())){
        throw End_Of_Archive();
    }
    assert( i < (_raw.c_str() + _raw.size()) );

    const uint64_t blob_length = *( (uint64_t *)(i + sizeof(uint64_t)) );

    const size_t new_index = _index + sizeof(uint64_t)*2 + blob_length;
    assert( new_index < _raw.size() );

    _index = new_index;
}
