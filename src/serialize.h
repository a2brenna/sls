#ifndef __SERIALIZE_H__
#define __SERIALIZE_H__

#include <vector>
#include <string>

std::vector<std::pair<uint64_t, std::string>> de_serialize(const std::string &raw);

#endif
