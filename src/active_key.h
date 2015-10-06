#ifndef __ACTIVE_KEY_H__
#define __ACTIVE_KEY_H__

#include "file.h"
#include "archive.h"
#include <mutex>
#include <chrono>

class Active_Key {

public:
  Active_Key(const Path &base_dir, const std::string &key, const bool &mkdir);
  Active_Key(const Active_Key &f) = delete;
  ~Active_Key();
  void append(const std::string &new_val);
  void append(const std::string &new_val,
              const std::chrono::milliseconds &time);
  void append_archive(const sls::Archive &archive);
  void sync();
  size_t num_elements() const;
  sls::Archive index_lookup(const size_t &start, const size_t &end) const;
  sls::Archive time_lookup(const std::chrono::milliseconds &begin,
                          const std::chrono::milliseconds &end) const;
  sls::Archive last_lookup(const size_t &max_values) const;

private:
  mutable std::mutex _lock;
  Path _directory;
  Path _index;
  std::string _key;
  std::string _name;
  size_t _start_pos;
  size_t _num_elements;
  size_t _last_element_start;
  size_t _filesize;
  bool _synced;
  std::chrono::milliseconds _last_time;

  Path _filepath() const;
  void _append(const std::string &new_val,
               const std::chrono::milliseconds &time);
  void _append_archive(const sls::Archive &archive);
  sls::Archive _index_lookup(const size_t &start, const size_t &end) const;
  void _sync();
};

#endif
