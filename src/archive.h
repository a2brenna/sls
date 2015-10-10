#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include "file.h"
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <smpl.h>

namespace sls{

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
        std::shared_ptr<char> _buffer;
        char* _cursor;
        char* _end;
        void _validate() const;

public:
  Archive(const Path &file);
  Archive(const Path &file, const size_t &offset);
  Archive(const Path &file, const size_t &offset, const size_t &max_size);
  Archive(const std::string &raw);
  Archive(smpl::Channel *channel);
  Archive();

  void advance_cursor();
  void set_offset(const size_t &offset);
  std::chrono::milliseconds head_time() const;
  std::string head_data() const;
  std::string head_record() const;
  std::vector<std::pair<std::chrono::milliseconds, std::string>> unpack() const;
  std::vector<std::string> extract() const;
  uint64_t cursor() const;
  const char *buffer() const;
  size_t size() const;
  sls::Metadata check() const;
  const std::string remainder() const;
  const std::string str() const;
  uint64_t checksum() const;
  size_t append(const std::chrono::milliseconds &timestamp, const std::string &value);
  size_t append(const Archive &archive);
  size_t append(const std::string &archive);
  size_t append(const Path &file, const size_t &offset);

};

}

#endif
