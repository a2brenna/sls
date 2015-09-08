#include "archive.h"
#include <fstream>
#include <cassert>
#include "file.h"

Archive::Archive(){
    _raw.clear();
    _index = 0;
    _last_time = std::chrono::milliseconds(0);
}

Archive::Archive(const Path &file) {
  _raw = readfile(file, 0, 0);
  _index = 0;
_last_time = std::chrono::milliseconds(0);
}

Archive::Archive(const Path &file, const size_t &offset) {
  _raw = readfile(file, offset, 0);
  _index = 0;
_last_time = std::chrono::milliseconds(0);
}

Archive::Archive(const std::string &raw) {
  _raw = raw;
  _index = 0;
_last_time = std::chrono::milliseconds(0);
}

uint64_t Archive::index() const { return _index; }

size_t Archive::size() const { return _raw.size(); }

std::chrono::milliseconds Archive::last_time() const {
    return _last_time;
}

std::chrono::milliseconds Archive::head_time() const {
  const char *i = _raw.c_str() + _index;
  if (i == (_raw.c_str() + _raw.size())) {
    throw End_Of_Archive();
  }
  assert(i < (_raw.c_str() + _raw.size()));

  const std::chrono::milliseconds timestamp(*((uint64_t *)(i)));
  return timestamp;
}

std::string Archive::head_data() const {
  const char *i = _raw.c_str() + _index;
  if (i == (_raw.c_str() + _raw.size())) {
    throw End_Of_Archive();
  }
  assert(i < (_raw.c_str() + _raw.size()));

  const uint64_t blob_length = *((uint64_t *)(i + sizeof(uint64_t)));
  const char *blob_start = i + sizeof(uint64_t) * 2;

  return std::string(blob_start, blob_length);
}

std::string Archive::head_record() const {
  const char *i = _raw.c_str() + _index;
  const char *end_of_archive = _raw.c_str() + _raw.size();
  if (i == end_of_archive) {
    throw End_Of_Archive();
  }

  const uint64_t data_length = *((uint64_t *)(i + sizeof(uint64_t)));
  const size_t record_length = sizeof(uint64_t) * 2 + data_length;

  assert(i < end_of_archive);
  assert(record_length > 0);
  return std::string(i, record_length);
}

Metadata Archive::check() const {
  uint64_t last_index = 0;
  size_t num_elements = 0;
  uint64_t milliseconds_from_epoch = 0;

  uint64_t index = 0;
  for (;;) {

    const char *i = _raw.c_str() + index;
    // Check that index is inside Archive
    if (i == (_raw.c_str() + _raw.size())) {
      break;
    } else if (i > (_raw.c_str() + _raw.size())) {
      throw Bad_Archive();
    }
    assert(i < (_raw.c_str() + _raw.size()));

    // Ensure time goes forward
    const uint64_t new_milliseconds_from_epoch = *((uint64_t *)(i));
    if (new_milliseconds_from_epoch < milliseconds_from_epoch) {
      throw Bad_Archive();
    }

    milliseconds_from_epoch = new_milliseconds_from_epoch;
    last_index = index;
    num_elements++;

    const uint64_t blob_length = *((uint64_t *)(i + sizeof(uint64_t)));
    index = index + sizeof(uint64_t) * 2 + blob_length;
  }

  Metadata m;
  m.elements = num_elements;
  m.index = last_index;
  m.timestamp = std::chrono::milliseconds(milliseconds_from_epoch);

  return m;
}

std::vector<std::pair<std::chrono::milliseconds, std::string>>
Archive::extract() const {
  const char *i = _raw.c_str() + _index;
  if (i == (_raw.c_str() + _raw.size())) {
    throw End_Of_Archive();
  }
  assert(i < (_raw.c_str() + _raw.size()));

  std::vector<std::pair<std::chrono::milliseconds, std::string>> output;

  const char *end = _raw.c_str() + _raw.size();
  assert(*end == '\0');

  while (i < end) {
    const std::chrono::milliseconds timestamp(*((uint64_t *)(i)));
    i += sizeof(uint64_t);

    const uint64_t blob_length = *((uint64_t *)(i));
    i += sizeof(uint64_t);

    output.push_back(std::pair<std::chrono::milliseconds, std::string>(
        timestamp, std::string(i, blob_length)));
    i += blob_length;
  }

  return output;
}

const std::string Archive::str() const{
    return _raw;
}

void Archive::advance_index() {
  const char *i = _raw.c_str() + _index;
  if (i == (_raw.c_str() + _raw.size())) {
    throw End_Of_Archive();
  }
  assert(i < (_raw.c_str() + _raw.size()));

  const uint64_t blob_length = *((uint64_t *)(i + sizeof(uint64_t)));

  const size_t new_index = _index + sizeof(uint64_t) * 2 + blob_length;
  assert(new_index <= _raw.size());

  _index = new_index;
}

void Archive::set_offset(const size_t &offset) { _index = offset; }

size_t Archive::append(const std::chrono::milliseconds &timestamp, const std::string &value){
    if(timestamp < _last_time){
        throw Out_Of_Order();
    }

    const uint64_t milliseconds_since_epoch = timestamp.count();
    const uint64_t value_size = value.size();

    _raw.append( (const char *)(&milliseconds_since_epoch), sizeof(uint64_t) );
    _raw.append( (const char *)(&value_size), sizeof(uint64_t) );
    _raw.append(value);

    _last_time = timestamp;
    return value.size() + (2 *sizeof(uint64_t));
}

size_t Archive::append(const Archive &archive){
    const std::chrono::milliseconds next_timestamp = archive.head_time();
    if(next_timestamp < _last_time){
        throw Out_Of_Order();
    }
    else{
        _raw.append(archive.str());
        _last_time = archive.last_time();
        return archive.size();
    }
}
