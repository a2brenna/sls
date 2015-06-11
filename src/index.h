#ifndef __INDEX_H__
#define __INDEX_H__

#include <string>
#include <vector>

#include <ostream>
#include <istream>

class Index_Record{
    private:
        uint64_t _timestamp;
        std::string _filename;
        uint64_t _offset;

    public:
        Index_Record(const uint64_t &timestamp, const std::string &filename, const uint64_t &offset);
        Index_Record();

        uint64_t timestamp() const;
        std::string filename() const;
        uint64_t offset() const;

};

typedef std::vector<Index_Record> Index;

std::ostream& operator<<(std::ostream& out, const Index_Record &i);
std::istream& operator>>(std::istream& in, Index_Record &i);

std::ostream& operator<<(std::ostream& out, const Index &i);
std::istream& operator>>(std::istream& in, Index &i);

std::vector<Index_Record> build_index(const std::string &directory);

#endif
