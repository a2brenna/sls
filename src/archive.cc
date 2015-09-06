#include "archive.h"
#include <fstream>
#include <cassert>
#include "file.h"

Archive::Archive(const Path &file) {
  _raw = readfile(file, 0, 0);
  _index = 0;
}

Archive::Archive(const Path &file, const size_t &offset) {
  _raw = readfile(file, offset, 0);
  _index = 0;
}

Archive::Archive(const std::string &raw) {
  _raw = raw;
  _index = 0;
}

uint64_t Archive::index() const { return _index; }

size_t Archive::size() const { return _raw.size(); }

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
