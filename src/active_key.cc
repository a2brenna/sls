#include "active_key.h"

#include <fstream>
#include <cassert>
#include <city.h>
#include <sys/stat.h>
#include "archive.h"
#include "index.h"
#include "util.h"
#include "config.h"
#include <chrono>

std::string bucket_dir(const std::string &key){
    return std::to_string(CityHash64(key.c_str(), key.size()) % CONFIG_NUM_BUCKETS);
}

Active_Key::Active_Key(const Path &base_dir, const std::string &key, const bool &make_directory):
    _directory(base_dir.str() + bucket_dir(key) + "/" + key + "/"),
    _index(_directory.str() + "index") {
    if(make_directory){
        const auto d = mkdir(_directory.str().c_str(), 0755);
        if (d != 0) {
            if (errno != EEXIST) {
                assert(false);
            }
        }
    }
  _key = key;
  _name = RandomString(32);
  Index index(_index);
  _start_pos = index.num_elements();
  _num_elements = 0;
  _synced = true;
  _last_time = std::chrono::milliseconds(0);
  _filesize = 0;
  _last_element_start = 0;
}

Path Active_Key::_filepath() const {
  return Path(_directory.str() + _name);
}

Active_Key::~Active_Key() {
  std::unique_lock<std::mutex> l(_lock);
  _sync();
}

void Active_Key::_append(const std::string &new_val,
                         const std::chrono::milliseconds &time) {
  if (_last_time > time) {
    throw sls::Out_Of_Order();
  }

  const uint64_t val_length = new_val.size();

  std::ofstream o(_filepath().str(),
                  std::ofstream::app | std::ofstream::binary);
  assert(o);

  const uint64_t c_time = time.count();
  o.write((char *)&c_time, sizeof(uint64_t));
  assert(o);
  o.write((char *)&val_length, sizeof(uint64_t));
  assert(o);
  o.write(new_val.c_str(), val_length);
  assert(o);

  o.close();

  const auto written = (2 * sizeof(uint64_t)) + val_length;
  _last_element_start = _filesize;
  _filesize = _filesize + written;

  _last_time = time;
  _synced = false;
  _num_elements++;

  if (_num_elements > 0) {
    if (_num_elements == 1) {
      _sync();
    } else if ((_num_elements % CONFIG_RESOLUTION) == 0) {
      _sync();
    }
  }
}

void Active_Key::_append_archive(const sls::Archive &archive) {
  const auto elements_past_index_point = _num_elements % CONFIG_RESOLUTION;

  //Ensure new data comes after existing data
  try {
    const auto archive_start_time = archive.head_time();
    if (archive_start_time < _last_time) {
      throw sls::Out_Of_Order();
    }
  } catch (sls::End_Of_Archive e) {
    throw sls::Bad_Archive();
  }

  // verify packed archive and take some measurements
  const sls::Metadata archive_metadata = archive.check();

  std::ofstream o(_filepath().str(),
                  std::ofstream::app | std::ofstream::binary);
  assert(o);

  o.write(archive.str().c_str(), archive.size());
  assert(o);

  o.close();

  const auto written = archive.size();
  _filesize = _filesize + written;
  _last_element_start = _filesize + archive_metadata.index;
  _last_time = archive_metadata.timestamp;
  _num_elements = _num_elements + archive_metadata.elements;

  _synced = false;

  if (_num_elements > 0) {
    if (_num_elements == 1) {
      _sync();
    } else if ((elements_past_index_point + archive_metadata.elements) >
               CONFIG_RESOLUTION) {
      _sync();
    }
  }
}

void Active_Key::append(const std::string &new_val,
                        const std::chrono::milliseconds &time) {
  std::unique_lock<std::mutex> l(_lock);
  _append(new_val, time);
}

void Active_Key::append(const std::string &new_val) {
  const auto current_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch());

  std::unique_lock<std::mutex> l(_lock);
  _append(new_val, current_time);
}

void Active_Key::append_archive(const sls::Archive &archive) {
  std::unique_lock<std::mutex> l(_lock);
  _append_archive(archive);
}

void Active_Key::_sync() {
  if (_synced) {
    return;
  }

  assert(_num_elements > 0);

  // Check the index on disk in case its changed since we instantiated this
  // Active_File
  Index index(_index);
  const auto relevant_records = index.get_records(_name);
  if (!relevant_records.empty()) {
    _start_pos = index.get_records(_name).front().position();
  }

  Index_Record record(_last_time, _start_pos + _num_elements - 1, _name,
                      _last_element_start);
  std::ofstream o(_index.str(), std::ofstream::app | std::ofstream::binary);
  assert(o);
  o << record;
  assert(o);
  o.close();

  _synced = true;
}

void Active_Key::sync() {
  std::unique_lock<std::mutex> l(_lock);
  _sync();
}

size_t Active_Key::num_elements() const {
  std::unique_lock<std::mutex> l(_lock);
  return _num_elements;
}

sls::Archive
Active_Key::time_lookup(const std::chrono::milliseconds &start,
                        const std::chrono::milliseconds &end) const {
  std::unique_lock<std::mutex> l(_lock);

  sls::Archive result;

  Index index(_index);
  std::vector<Index_Record> files = index.time_lookup(start, end);
  if (files.empty()) {
    return result;
  }
  else{
      for(const auto &f: files){
        *Info << "Reading file: " << f.filename() << std::endl;
      }
  }

  const auto start_time = std::chrono::high_resolution_clock::now();

    if(files.size() > 1){
        try{
            const auto f = files[0];
            Path path(_directory.str() + f.filename());
            sls::Archive temp(path, f.offset());
            std::chrono::milliseconds current_time = f.timestamp();
            while(current_time < start){
                temp.advance_cursor();
                current_time = temp.head_time();
            }
            result.append(temp.remainder());
        }
        catch(sls::End_Of_Archive e){
            //This can occur when start_time appears between end of one file and beginning of next...
            //Timestamps are not dense
        }
    }
    if(files.size() > 2){
        for(size_t i = 1; i < (files.size() - 1); i++){
            const auto f = files[i];
            Path path(_directory.str() + f.filename());
            result.append(path, 0);
        }
    }
    if(files.size() > 0){
        const auto f = files.back();
        Path path(_directory.str() + f.filename());
        sls::Archive arch(path);
        arch.set_offset(f.offset());
        std::chrono::milliseconds current_time = f.timestamp();
        while (current_time <= end) {
            try{
                if (current_time < start) {
                    arch.advance_cursor();
                    current_time = arch.head_time();
                }
                else {
                    result.append(arch.head_record());
                    arch.advance_cursor();
                    current_time = arch.head_time();
                }
            } catch (sls::End_Of_Archive e){
                break;
            }
        }
    }
  return result;
}

sls::Archive Active_Key::index_lookup(const size_t &start,
                                     const size_t &end) const {
  std::unique_lock<std::mutex> l(_lock);
  return _index_lookup(start, end);
}

sls::Archive Active_Key::_index_lookup(const size_t &start,
                                      const size_t &upper_bound) const {
  const size_t end = std::min(upper_bound, _total_elements());

  assert(start <= end);

  sls::Archive result;

  Index index(_index);
  std::vector<Index_Record> files = index.position_lookup(start, end);
  if (files.empty()) {
    return result;
  }
  else{
      for(const auto &f: files){
        *Info << "Reading file: " << f.filename() << std::endl;
      }
  }

  const auto start_time = std::chrono::high_resolution_clock::now();

    if(files.size() > 1){
        try{
            const auto f = files[0];
            Path path(_directory.str() + f.filename());
            sls::Archive temp(path, f.offset());
            size_t current_index = f.position();
            while(current_index < start){
                temp.advance_cursor();
                current_index++;
            }
            result.append(temp.remainder());
        }
        catch(sls::End_Of_Archive e){
            //This should be impossible and indicates that the index_lookup is borked
            //indexes ARE dense
            assert(false);
        }
    }
    if(files.size() > 2){
        for(size_t i = 1; i < (files.size() - 1); i++){
            const auto f = files[i];
            Path path(_directory.str() + f.filename());
            result.append(path, 0);
        }
    }
    if(files.size() > 0){
        const auto f = files.back();
        Path path(_directory.str() + f.filename());
        sls::Archive arch(path);
        arch.set_offset(f.offset());
        size_t current_index = f.position();
        while (current_index <= end) {
            if (current_index < start) {
                arch.advance_cursor();
                current_index++;
            }
            else {
                result.append(arch.head_record());
                arch.advance_cursor();
                current_index++;
            }
        }
    }

  return result;
}

sls::Archive Active_Key::last_lookup(const size_t &max_values) const {
  std::unique_lock<std::mutex> l(_lock);
  const int total_elements = _num_elements + _start_pos;
  const int last_index = total_elements - 1;
  const int start_index =
      std::max(0, last_index - (int)max_values +
                      1); // +1 because lookups are inclusive on index
  return _index_lookup(start_index, last_index);
}

size_t Active_Key::_total_elements() const{
    return _num_elements + _start_pos;
}
