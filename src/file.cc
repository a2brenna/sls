#include "file.h"

#include <dirent.h>
#include <string.h>
#include <fstream>

int getdir(std::string dir, std::vector<std::string> &files) {
  struct dirent *dirp;
  DIR *dp = opendir(dir.c_str());
  if (dp == nullptr) {
      return -1;
  }

  while ((dirp = readdir(dp)) != nullptr) {
    if (strcmp(dirp->d_name, "..") == 0) {
      continue;
    }
    if (strcmp(dirp->d_name, ".") == 0) {
      continue;
    }
    files.push_back(std::string(dirp->d_name));
  }
  closedir(dp);
  return 0;
}

std::string readfile(const Path &filepath, const size_t &offset, const size_t &max_size){
    std::string output;
    std::ifstream i(filepath.str(), std::ios_base::in);
    i.seekg(offset, i.beg);

    if(max_size > 0){
        output.resize(max_size);
        i.get(&output[0], max_size);
    }
    else{
        i >> output;
    }

    return output;
}



