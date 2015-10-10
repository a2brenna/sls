#include "archive.h"
#include <fstream>
#include <cassert>
#include "file.h"
#include <cstring>
#include <iostream>
#include <city.h>

#define MAX_ARCHIVE_SIZE 1073741824

namespace sls{

Archive::Archive():
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        _cursor = _buffer.get();
        _end = _buffer.get();
}

Archive::Archive(const Path &file):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        const auto archive_size = readfile(file, 0, _buffer.get(), MAX_ARCHIVE_SIZE);
        if(archive_size < 0){
            throw Bad_Archive();
        }
        else{
            _cursor = _buffer.get();
            _end = _buffer.get() + archive_size;
        }
}

Archive::Archive(const Path &file, const size_t &offset):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        const auto archive_size = readfile(file, offset, _buffer.get(), MAX_ARCHIVE_SIZE);
        if(archive_size < 0){
            throw Bad_Archive();
        }
        else{
            _cursor = _buffer.get();
            _end = _buffer.get() + archive_size;
        }
}

Archive::Archive(smpl::Channel *channel):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        try{
            _cursor = _buffer.get();
            const size_t archive_size = channel->recv(_buffer.get(), MAX_ARCHIVE_SIZE);
            _end = _buffer.get() + archive_size;
        }
        catch(smpl::Transport_Failed e){
            throw Bad_Archive();
        }
}

Archive::Archive(const std::string &raw):
    _buffer(new char[MAX_ARCHIVE_SIZE]){
        if(raw.size() > MAX_ARCHIVE_SIZE){
            throw Bad_Archive();
        }
        else{
            memcpy(_buffer.get(), &raw[0], raw.size());

            _cursor = _buffer.get();
            _end = _buffer.get() + raw.size();
        }
}

uint64_t Archive::checksum() const{
    return CityHash64(_buffer.get(), size());
}

void Archive::_validate() const{
    assert(_cursor <= _end);
    assert(_buffer.get() <= _cursor);
}


uint64_t Archive::cursor() const { return (_cursor - _buffer.get()); }

size_t Archive::size() const { return (_end - _buffer.get()); }

const char* Archive::buffer() const{
    return _buffer.get();
}

std::chrono::milliseconds Archive::head_time() const {
  _validate();
  if (_cursor == _end  ) {
    throw End_Of_Archive();
  }

  const std::chrono::milliseconds timestamp(*((uint64_t *)(_cursor)));
    _validate();
  return timestamp;
}

std::string Archive::head_data() const {
    _validate();
  if (_cursor == _end) {
    throw End_Of_Archive();
  }

  const uint64_t blob_length = *((uint64_t *)(_cursor + sizeof(uint64_t)));
  const char *blob_start = _cursor + sizeof(uint64_t) * 2;

    _validate();
  return std::string(blob_start, blob_length);
}

std::string Archive::head_record() const {
    _validate();
  if (_cursor == _end) {
    throw End_Of_Archive();
  }

  const uint64_t data_length = *((uint64_t *)(_cursor + sizeof(uint64_t)));
  const size_t record_length = sizeof(uint64_t) * 2 + data_length;

  assert(record_length > 0);
  assert(_cursor + record_length <= _end);
    _validate();
  return std::string(_cursor, record_length);
}

Metadata Archive::check() const {
    _validate();
  uint64_t last_offset = 0;
  size_t num_elements = 0;
  uint64_t milliseconds_from_epoch = 0;

  char *cursor = _buffer.get();
  for (;;) {

    // Check that index is inside Archive
    if (cursor == _end) {
      break;
    } else if (cursor > _end) {
      throw Bad_Archive();
    }

    // Ensure time goes forward
    const uint64_t new_milliseconds_from_epoch = *((uint64_t *)(cursor));
    if (new_milliseconds_from_epoch == 0){
        throw Bad_Archive();
    }
    else if (new_milliseconds_from_epoch < milliseconds_from_epoch) {
        throw Bad_Archive();
    }

    milliseconds_from_epoch = new_milliseconds_from_epoch;
    last_offset = cursor - _buffer.get();
    num_elements++;

    const uint64_t blob_length = *((uint64_t *)(cursor + sizeof(uint64_t)));
    cursor = cursor + sizeof(uint64_t) * 2 + blob_length;
  }

  sls::Metadata m;
  m.elements = num_elements;
  m.index = last_offset;
  m.timestamp = std::chrono::milliseconds(milliseconds_from_epoch);

    _validate();
  return m;
}

std::vector<std::pair<std::chrono::milliseconds, std::string>> Archive::unpack() const {
  _validate();

  if (_cursor == _end) {
    throw End_Of_Archive();
  }

  const char* cursor = _cursor;
    std::vector<std::pair<std::chrono::milliseconds, std::string>> output;

  while(cursor < _end) {
    const std::chrono::milliseconds timestamp(*((uint64_t *)(cursor)));
    cursor += sizeof(uint64_t);

    const size_t blob_length = *((uint64_t *)(cursor));
    cursor += sizeof(uint64_t);

    output.push_back(std::pair<std::chrono::milliseconds, std::string>(timestamp, std::string(cursor, blob_length)));
    cursor += blob_length;
  }
    _validate();
    return output;
}

std::vector<std::string> Archive::extract() const {
    _validate();
  if (_cursor == _end) {
    throw End_Of_Archive();
  }
  const char* cursor = _cursor;

  std::vector<std::string> output;

  while (cursor < _end) {
    cursor += sizeof(uint64_t);

    const uint64_t blob_length = *((uint64_t *)(cursor));
    cursor += sizeof(uint64_t);

    output.push_back(std::string(cursor, blob_length));
    cursor += blob_length;
  }

    _validate();
  return output;
}

const std::string Archive::str() const{
    _validate();
    std::string foo;
    foo.reserve(size());
    foo.append(_buffer.get(), size());
    _validate();
    return foo;
}

void Archive::advance_cursor() {
    _validate();
  if (_cursor == _end) {
    throw End_Of_Archive();
  }

  const uint64_t blob_length = *((uint64_t *)(_cursor + sizeof(uint64_t)));
  _cursor = _cursor + sizeof(uint64_t) * 2 + blob_length;

  _validate();
}

void Archive::set_offset(const size_t &offset) {
    _validate();
    _cursor = _buffer.get() + offset;
    _validate();
}

const std::string Archive::remainder() const{
    _validate();
    return std::string(_cursor, _end - _cursor);
}

size_t Archive::append(const std::chrono::milliseconds &timestamp, const std::string &value){
    _validate();
    const uint64_t milliseconds_since_epoch = timestamp.count();
    const uint64_t value_size = value.size();

    const size_t bytes_to_copy = 2 * sizeof(uint64_t) + value.size();

    assert( (size() + bytes_to_copy) < MAX_ARCHIVE_SIZE);

    size_t bytes_copied = 0;
    memcpy(_end + bytes_copied, (const char *)(&milliseconds_since_epoch), sizeof(uint64_t));
    bytes_copied += sizeof(uint64_t);
    memcpy(_end + bytes_copied, (const char *)(&value_size), sizeof(uint64_t));
    bytes_copied += sizeof(uint64_t);
    memcpy(_end + bytes_copied, (const char *)(&value[0]), value.size());
    bytes_copied += value.size();

    assert(bytes_copied == bytes_to_copy);
    _end = _end + bytes_copied;

    _validate();
    return bytes_copied;
}

size_t Archive::append(const sls::Archive &archive){
    _validate();
    if( (size() + archive.size()) > MAX_ARCHIVE_SIZE){
        throw Bad_Archive();
    }
    memcpy(_end, &archive.str()[0], archive.size());
    _end = _end + archive.size();
    _validate();
    return archive.size();
}

size_t Archive::append(const std::string &archive){
    _validate();
    if( (size() + archive.size()) > MAX_ARCHIVE_SIZE){
        throw Bad_Archive();
    }
    memcpy(_end, &archive[0], archive.size());
    _end = _end + archive.size();
    _validate();
    return archive.size();
}

size_t Archive::append(const Path &file, const size_t &offset){
    _validate();
    const auto chars_read = readfile(file, offset, _end, MAX_ARCHIVE_SIZE);
    if(chars_read < 0){
        throw Bad_Archive();
    }
    else{
        _end = _end + chars_read;
        _validate();
        return chars_read;
    }
}

}
