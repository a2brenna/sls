#ifndef __FILE_H__
#define __FILE_H__

#include <string>
#include <vector>

class Path {

public:
  Path(const std::string &p) { _path = p; };
  const std::string str() const { return _path; };

private:
  std::string _path;
};

int getdir(std::string dir, std::vector<std::string> &files);

std::string readfile(const Path &filepath, const size_t &offset,
                     const size_t &max_size);

#endif
