#include "active_key.h"

#include <fstream>
#include <cassert>
#include "archive.h"
#include "index.h"
#include "util.h"
#include "config.h"

Active_Key::Active_Key(const Path &base_dir, const std::string &key):
    _base_dir(base_dir),
    _index(base_dir.str() + key +"/index")
{
    _key = key;
    _name = RandomString(32);
    Index index(_index);
    _start_pos = index.num_elements();
    _num_elements = 0;
    _synced = true;
    _last_time = 0;
    _filesize = 0;
    _last_element_start = 0;
}

Path Active_Key::_filepath() const{
    return Path(_base_dir.str() + _key + "/" + _name);
}

Active_Key::~Active_Key(){
    std::unique_lock<std::mutex> l(_lock);
    _sync();
}

void Active_Key::append(const std::string &new_val){
    std::unique_lock<std::mutex> l(_lock);
    const uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    const uint64_t val_length = new_val.size();

    std::ofstream o(_filepath().str(), std::ofstream::app | std::ofstream::binary);
    assert(o);

    o.write((char *)&current_time, sizeof(uint64_t));
    assert(o);
    o.write((char *)&val_length, sizeof(uint64_t));
    assert(o);
    o.write(new_val.c_str(), val_length);
    assert(o);

    o.close();

    const auto written = (2 * sizeof(uint64_t)) + val_length;
    _last_element_start = _filesize;
    _filesize = _filesize + written;

    _last_time = current_time;
    _synced = false;
    _num_elements++;

    if(_num_elements > 0){
        if( (_num_elements == 1) || ((_num_elements % CONFIG_RESOLUTION) == 0) ){
            _sync();
        }
    }
}

void Active_Key::_sync(){
    if( _synced ){
        return;
    }

    Index_Record record(_last_time, _start_pos + _num_elements - 1, _name, _last_element_start);
    assert(_num_elements > 0);
    std::ofstream o(_index.str(), std::ofstream::app | std::ofstream::binary);
    assert(o);
    o << record;
    assert(o);
    o.close();

    _synced = true;

}

void Active_Key::sync(){
    std::unique_lock<std::mutex> l(_lock);
    _sync();
}

size_t Active_Key::num_elements() const{
    std::unique_lock<std::mutex> l(_lock);
    return _num_elements;
}

std::string Active_Key::time_lookup(const uint64_t &start, const uint64_t &end){
    std::unique_lock<std::mutex> l(_lock);

    std::string result;

    Index index(_index);
    std::vector<Index_Record> files = index.time_lookup(start, end);
    if(files.empty()){
        return result;
    }
    for(const auto &f: files){
        Path path(_base_dir.str() + _key + "/" + f.filename());
        Archive arch(path);
        arch.set_index(f.offset());
        while(true){
            try{
                const uint64_t current_time = arch.head_time();

                if( current_time < start ){
                    arch.advance_index();
                }
                else if( current_time > end){
                    break;
                }
                else if( current_time >= start && current_time <= end){
                    result.append(arch.head_record());
                    arch.advance_index();
                }
                else{
                    assert(false);
                }
            }
            catch(End_Of_Archive e){
                break;
            }
        }
    }
    return result;
}

std::string Active_Key::index_lookup(const size_t &start, const size_t &end){
    std::unique_lock<std::mutex> l(_lock);
    return _index_lookup(start, end);
}

std::string Active_Key::_index_lookup(const size_t &start, const size_t &end){
    assert(start <= end);
    std::string result;

    Index index(_index);
    std::vector<Index_Record> files = index.position_lookup(start, end);
    if(files.empty()){
        return result;
    }
    for(const auto &f: files){
        size_t current_index = f.position();
        Path path(_base_dir.str() + _key + "/" + f.filename());
        Archive arch(path);
        arch.set_index(f.offset());
        while(true){
            try{
                if( current_index < start){
                    arch.advance_index();
                    current_index++;
                }
                else if( current_index > end){
                    break;
                }
                else if( current_index >= start && current_index <= end){
                    result.append(arch.head_record());
                    arch.advance_index();
                    current_index++;
                }
                else{
                    assert(false);
                }
            }
            catch(End_Of_Archive e){
                break;
            }
        }
    }
    return result;
}

std::string Active_Key::last_lookup( const size_t &max_values){
    std::unique_lock<std::mutex> l(_lock);
    const int total_elements = _num_elements + _start_pos;
    const int last_index = total_elements - 1;
    const int start_index = std::max(0, last_index - (int)max_values + 1); // +1 because lookups are inclusive on index
    return _index_lookup(start_index, last_index);
}

