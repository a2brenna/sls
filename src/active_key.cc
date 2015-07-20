#include "active_key.h"

#include <hgutil/strings.h>
#include <fstream>
#include <cassert>
#include "archive.h"
#include "index.h"

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
    _last_element_offset = 0;
}

Path Active_Key::_filepath() const{
    return Path(_base_dir.str() + _key + "/" + _name);
}

void Active_Key::append(const std::string &new_val){
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

void Active_Key::sync(){
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
    Index index(_index);
    const size_t total_elements = _num_elements + index.num_elements();
    const size_t upper_bound = total_elements + 1;

    const int lower_bound_index = upper_bound - max_values;
    const size_t lower_bound = std::max(0, lower_bound_index);

    return _index_lookup(lower_bound, upper_bound);
}
