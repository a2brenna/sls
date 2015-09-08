#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include "file.h"
#include <vector>
#include <string>
#include <chrono>

class End_Of_Archive {};

class Bad_Archive {};

class Out_Of_Order {};

class Metadata {
public:
  size_t elements = 0;
  uint64_t index = 0;
  std::chrono::milliseconds timestamp;
};

class Archive {

    private:
        std::chrono::milliseconds _last_time;

public:
  Archive(const Path &file);
  Archive(const Path &file, const size_t &offset);
  Archive(const Path &file, const size_t &offset, const size_t &max_size);
  Archive(const std::string &raw);
  Archive();

  void advance_index();
  void set_offset(const size_t &offset);
  std::chrono::milliseconds head_time() const;
  std::chrono::milliseconds last_time() const;
  std::string head_data() const;
  std::string head_record() const;
  std::vector<std::pair<std::chrono::milliseconds, std::string>>
  extract() const;
  uint64_t index() const;
  size_t size() const;
  Metadata check() const;
  const std::string str() const;
  size_t append(const std::chrono::milliseconds &timestamp, const std::string &value);
  size_t append(const Archive &archive);

private:
  std::string _raw;
  size_t _index;
};

#endif
