#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include "file.h"
#include <vector>
#include <string>

class End_Of_Archive {};

class Archive{

    public:
        Archive(const Path &file);
        Archive(const std::string &raw);

        uint64_t head_time() const;
        std::string head_data() const;
        std::string head_record() const;
        std::vector<std::pair<uint64_t, std::string>> extract() const;
        uint64_t index() const;
        void advance_index();

    private:
        std::string _raw;
        size_t _index;

};

#endif
