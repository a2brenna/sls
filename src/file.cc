#include "file.h"

#include <dirent.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

size_t MAX_SIZE = 1073741824;

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

std::string readfile(const Path &filepath, const size_t &offset){
    std::string msg;
    msg.resize(MAX_SIZE);
    size_t read = 0;
    char *buff = &msg[0];

    int _fd = ::open(filepath.str().c_str(), O_RDONLY);
    ::lseek(_fd, offset, SEEK_SET);

    for(;;) {
        const size_t to_read = MAX_SIZE - read;

        const auto ret = ::read(_fd, (buff + read), to_read);

        if (ret == 0){
            break;
        }
        else if(ret < 0){
            msg.clear();
            close(_fd);
            return msg;
        }
        else{
            read = read + ret;
        }
    }

    msg.resize(read);
    close(_fd);
    return msg;
}

ssize_t readfile(const Path &filepath, const size_t &offset, char *buffer, const size_t &max_size){

    int _fd = ::open(filepath.str().c_str(), O_RDONLY);
    ::lseek(_fd, offset, SEEK_SET);

    ssize_t read = 0;
    for(;;) {
        const size_t to_read = max_size - read;

        const auto ret = ::read(_fd, (buffer + read), to_read);

        if (ret == 0){
            close(_fd);
            return read;
        }
        else if(ret < 0){
            close(_fd);
            return ret;
        }
        else{
            read = read + ret;
        }
    }
}
