#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include "file.h"
#include <vector>
#include <string>
#include <chrono>

class End_Of_Archive {};

class Archive{

    public:
        Archive(const Path &file);
        Archive(const Path &file, const size_t &offset);
        Archive(const Path &file, const size_t &offset, const size_t &max_size);
        Archive(const std::string &raw);

        void advance_index();
        void set_offset(const size_t &offset);
        std::chrono::milliseconds head_time() const;
        std::string head_data() const;
        std::string head_record() const;
        std::vector<std::pair<std::chrono::milliseconds, std::string>> extract() const;
        uint64_t index() const;
        size_t size() const;

    private:
        std::string _raw;
        size_t _index;

};

#endif
