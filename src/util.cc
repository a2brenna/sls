#include "util.h"

std::string RandomString(unsigned int len) {
  std::string str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string result = "";
  while (result.size() != len) {
    result.append(1, (str[rand() % 62]));
  }
  return result;
}
