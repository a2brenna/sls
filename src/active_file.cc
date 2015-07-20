#include "active_file.h"

#include <hgutil/strings.h>
#include <chrono>
#include <fstream>
#include <cassert>
#include "index.h"

Active_File::Active_File(const Path &base_dir, const std::string &key, const size_t &start_pos):
    _base_dir(base_dir),
    _index(base_dir.str() + key +"/index")
{
    _key = key;
    _name = RandomString(32);
    _start_pos = start_pos;
    _num_elements = 0;
    _synced = true;
    _last_time = 0;
    _filesize = 0;
    _last_element_offset = 0;
}

Path Active_File::_filepath() const{
    return Path(_base_dir.str() + _key + "/" + _name);
}

void Active_File::append(const std::string &new_val){
    std::unique_lock<std::mutex> l(_lock);
    const uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    const uint64_t val_length = new_val.size();

    std::ofstream o(_filepath().str(), std::ofstream::app | std::ofstream::binary);
    assert(o);

    if(_filesize != 0){
        _last_element_offset = _filesize + 1;
    }

    o.write((char *)&current_time, sizeof(uint64_t));
    assert(o);
    o.write((char *)&val_length, sizeof(uint64_t));
    assert(o);
    o.write(new_val.c_str(), val_length);
    assert(o);

    o.close();
    _filesize += (_last_element_offset + (2 * sizeof(uint64_t)) + val_length);

    _last_time = current_time;
    _synced = false;
    _num_elements++;

    if(_num_elements == 1){
        sync();
    }
}

void Active_File::sync(){
    std::unique_lock<std::mutex> l(_lock);
    assert(_num_elements > 0);

    if( _synced ){
        return;
    }

    Index_Record record(_last_time, _start_pos + _num_elements - 1, _name, _last_element_offset);
    std::ofstream o(_index.str(), std::ofstream::app | std::ofstream::binary);
    assert(o);
    o << record;
    assert(o);
    o.close();

    _synced = true;
}

size_t Active_File::num_elements() const{
    std::unique_lock<std::mutex> l(_lock);
    return _num_elements;
}
