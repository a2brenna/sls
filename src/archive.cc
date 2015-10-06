#include "archive.h"
#include <fstream>
#include <cassert>
#include "file.h"
#include <cstring>
#include <iostream>

#define MAX_ARCHIVE_SIZE 1073741824

namespace sls{

Archive::Archive():
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        _index = 0;
        _size = 0;
}

Archive::Archive(const Path &file):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        const auto size = readfile(file, 0, _buffer.get(), MAX_ARCHIVE_SIZE);
        if(size < 0){
            throw Bad_Archive();
        }
        else{
            _size = size;
            _index = 0;
        }
}

Archive::Archive(const Path &file, const size_t &offset):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        const auto size = readfile(file, offset, _buffer.get(), MAX_ARCHIVE_SIZE);
        if(size < 0){
            throw Bad_Archive();
        }
        else{
            _size = size;
            _index = 0;
        }
}

Archive::Archive(const std::string &raw):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        if(raw.size() > MAX_ARCHIVE_SIZE){
            throw Bad_Archive();
        }
        else{
            memcpy(_buffer.get(), &raw[0], raw.size());
            _size = raw.size();
            _index = 0;
        }
}

uint64_t Archive::index() const { return _index; }

size_t Archive::size() const { return _size; }

std::chrono::milliseconds Archive::head_time() const {
  const char *i = _buffer.get() + _index;
  if (i == (_buffer.get() + size())) {
    throw End_Of_Archive();
  }
  assert(i < (_buffer.get() + size()));

  const std::chrono::milliseconds timestamp(*((uint64_t *)(i)));
  return timestamp;
}

std::string Archive::head_data() const {
  const char *i = _buffer.get() + _index;
  if (i == (_buffer.get() + size())) {
    throw End_Of_Archive();
  }
  assert(i < (_buffer.get() + size()));

  const uint64_t blob_length = *((uint64_t *)(i + sizeof(uint64_t)));
  const char *blob_start = i + sizeof(uint64_t) * 2;

  return std::string(blob_start, blob_length);
}

std::string Archive::head_record() const {
  const char *i = _buffer.get() + _index;
  const char *end_of_archive = _buffer.get() + size();
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

    const char *i = _buffer.get() + index;
    // Check that index is inside Archive
    if (i == (_buffer.get() + size())) {
      break;
    } else if (i > (_buffer.get() + size())) {
        std::cerr << "index: " << index << " size(): " << size() << std::endl;
      throw Bad_Archive();
    }
    assert(i < (_buffer.get() + size()));

    // Ensure time goes forward
    const uint64_t new_milliseconds_from_epoch = *((uint64_t *)(i));
    if (new_milliseconds_from_epoch == 0){
        std::cerr << "Time ZERO at: " << index << std::endl;
        throw Bad_Archive();
    }
    else if (new_milliseconds_from_epoch < milliseconds_from_epoch) {
        std::cerr << "New time: " << new_milliseconds_from_epoch << " old_time: " << milliseconds_from_epoch << std::endl;
      throw Bad_Archive();
    }

    milliseconds_from_epoch = new_milliseconds_from_epoch;
    last_index = index;
    num_elements++;

    const uint64_t blob_length = *((uint64_t *)(i + sizeof(uint64_t)));
    index = index + sizeof(uint64_t) * 2 + blob_length;
  }

  sls::Metadata m;
  m.elements = num_elements;
  m.index = last_index;
  m.timestamp = std::chrono::milliseconds(milliseconds_from_epoch);

  return m;
}

std::vector<std::pair<std::chrono::milliseconds, std::string>>
Archive::unpack() const {
  const char *i = _buffer.get() + _index;
  if (i == (_buffer.get() + size())) {
    throw End_Of_Archive();
  }
  assert(i < (_buffer.get() + size()));

  std::vector<std::pair<std::chrono::milliseconds, std::string>> output;

  const char *end = _buffer.get() + size();
  assert(i < end);

  while (i < end) {
    const std::chrono::milliseconds timestamp(*((uint64_t *)(i)));
    i += sizeof(uint64_t);

    const uint64_t blob_length = *((uint64_t *)(i));
    i += sizeof(uint64_t);

    try{
    output.push_back(std::pair<std::chrono::milliseconds, std::string>(
        timestamp, std::string(i, blob_length)));
    }
    catch(std::bad_alloc e){
        throw Bad_Archive();
    }
    i += blob_length;
  }

  return output;
}

std::vector<std::string> Archive::extract() const {
  const char *i = _buffer.get() + _index;
  if (i == (_buffer.get() + size())) {
    throw End_Of_Archive();
  }
  assert(i < (_buffer.get() + size()));

  std::vector<std::string> output;

  const char *end = _buffer.get() + size();
  assert(*end == '\0');

  while (i < end) {
    i += sizeof(uint64_t);

    const uint64_t blob_length = *((uint64_t *)(i));
    i += sizeof(uint64_t);

    output.push_back(std::string(i, blob_length));
    i += blob_length;
  }

  return output;
}

const std::string Archive::str() const{
    std::string foo;
    foo.reserve(size());
    foo.append(_buffer.get(), size());
    return foo;
}

void Archive::advance_index() {
  const char *i = _buffer.get() + _index;
  if (i == (_buffer.get() + size())) {
    throw End_Of_Archive();
  }
  assert(i < (_buffer.get() + size()));

  const uint64_t blob_length = *((uint64_t *)(i + sizeof(uint64_t)));

  const size_t new_index = _index + sizeof(uint64_t) * 2 + blob_length;
  assert(new_index <= size());

  _index = new_index;
}

void Archive::set_offset(const size_t &offset) {
    assert(_index < size());
    _index = offset;
}

const std::string Archive::remainder() const{
    return std::string(_buffer.get() + _index, size());
}

size_t Archive::append(const std::chrono::milliseconds &timestamp, const std::string &value){
    const uint64_t milliseconds_since_epoch = timestamp.count();
    const uint64_t value_size = value.size();

    const size_t bytes_to_copy = 2 * sizeof(uint64_t) + value.size();
    size_t bytes_copied = 0;

    assert( (size() + bytes_to_copy) < MAX_ARCHIVE_SIZE);

    memcpy(_buffer.get() + size() + bytes_copied, (const char *)(&milliseconds_since_epoch), sizeof(uint64_t));
    bytes_copied += sizeof(uint64_t);
    memcpy(_buffer.get() + size() + bytes_copied, (const char *)(&value_size), sizeof(uint64_t));
    bytes_copied += sizeof(uint64_t);
    memcpy(_buffer.get() + size() + bytes_copied, (const char *)(&value[0]), value.size());
    bytes_copied += value.size();

    assert(bytes_copied == bytes_to_copy);
    _size = _size + bytes_copied;

    return bytes_copied;
}

size_t Archive::append(const sls::Archive &archive){
    if( (size() + archive.size()) > MAX_ARCHIVE_SIZE){
        std::cerr << "size(): " << size() << " archive.size(): " << archive.size() << " MAX_ARCHIVE_SIZE: " << MAX_ARCHIVE_SIZE << std::endl;
        throw Bad_Archive();
    }
    memcpy(_buffer.get(), &archive.str()[0], archive.size());
    _size = _size + archive.size();
    return archive.size();
}

size_t Archive::append(const std::string &archive){
    if( (size() + archive.size()) > MAX_ARCHIVE_SIZE){
        std::cerr << "size(): " << size() << " archive.size(): " << archive.size() << " MAX_ARCHIVE_SIZE: " << MAX_ARCHIVE_SIZE << std::endl;
        throw Bad_Archive();
    }
    memcpy(_buffer.get(), &archive[0], archive.size());
    _size = _size + archive.size();
    return archive.size();
}

size_t Archive::append(const Path &file, const size_t &offset){
    const auto size = readfile(file, offset, _buffer.get() + _size, MAX_ARCHIVE_SIZE);
    if(size < 0){
        throw Bad_Archive();
    }
    else{
        _size = _size + size;
        return size;
    }
}

}
