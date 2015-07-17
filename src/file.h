#ifndef __FILE_H__
#define __FILE_H__

#include <string>

class Path{

    public:
        Path(const std::string &p) { _path = p; };
        const std::string str() const { return _path; };

    private:
        std::string _path;

};

#endif
