#ifndef __ACTIVE_KEY_H__
#define __ACTIVE_KEY_H__

#include "file.h"
#include <mutex>

class Active_Key{

    public:
        Active_Key(const Path &base_dir, const std::string &key);
        Active_Key(const Active_Key &f) = delete;
        void append(const std::string &new_val);
        void sync();
        size_t num_elements() const;
        std::string index_lookup(const size_t &start, const size_t &end);
        std::string time_lookup(const std::chrono::high_resolution_clock::time_point &begin, const std::chrono::high_resolution_clock::time_point &end);
        std::string last_lookup(const size_t &max_values);

    private:
        mutable std::mutex _lock;
        Path _base_dir;
        std::string _key;
        std::string _name;

        Path _index;
        Path _filepath() const;
        size_t _start_pos;
        size_t _num_elements;
        size_t _last_element_offset;
        size_t _filesize;
        bool _synced;
        uint64_t _last_time;

};

#endif
