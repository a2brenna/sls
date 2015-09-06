#ifndef __ACTIVE_KEY_H__
#define __ACTIVE_KEY_H__

#include "file.h"
#include <mutex>
#include <chrono>

class Active_Key {

public:
  Active_Key(const Path &base_dir, const std::string &key);
  Active_Key(const Active_Key &f) = delete;
  ~Active_Key();
  void append(const std::string &new_val);
  void append(const std::string &new_val,
              const std::chrono::milliseconds &time);
  void sync();
  size_t num_elements() const;
  std::string index_lookup(const size_t &start, const size_t &end) const;
  std::string time_lookup(const std::chrono::milliseconds &begin,
                          const std::chrono::milliseconds &end) const;
  std::string last_lookup(const size_t &max_values) const;

private:
  mutable std::mutex _lock;
  Path _base_dir;
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
  std::string _index_lookup(const size_t &start, const size_t &end) const;
  void _initialize(const std::string key);
  void _sync();
};

#endif
