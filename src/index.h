#ifndef __INDEX_H__
#define __INDEX_H__

#include <string>
#include <vector>

#include <ostream>
#include <istream>

#include "file.h"

class Index_Record{
    private:
        uint64_t _timestamp;
        uint64_t _position;
        std::string _filename;
        uint64_t _offset;

    public:
        Index_Record(const uint64_t &timestamp, const uint64_t &position, const std::string &filename, const uint64_t &offset);
        Index_Record();

        uint64_t timestamp() const;
        uint64_t position() const;
        std::string filename() const;
        uint64_t offset() const;

};

bool operator<(const Index_Record &rhs, const Index_Record &lhs);
bool operator>(const Index_Record &rhs, const Index_Record &lhs);

class Index {

    public:
        const std::vector<Index_Record> &index() const;
        //TODO: make void?
        bool append(const Index_Record &r);

    private:
        std::vector<Index_Record> _index;

};

//typedef std::vector<Index_Record> Index;

std::ostream& operator<<(std::ostream& out, const Index_Record &i);
std::istream& operator>>(std::istream& in, Index_Record &i);

std::ostream& operator<<(std::ostream& out, const Index &i);
std::istream& operator>>(std::istream& in, Index &i);

Index build_index(const Path &directory);

#endif
