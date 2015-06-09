#include "serialize.h"
#include <vector>
#include <string>

std::vector<std::pair<uint64_t, std::string>> de_serialize(const std::string &raw){
    std::vector<std::pair<uint64_t, std::string>> output;

    const char *i = raw.c_str();
    const char *end = i + raw.size();

    while( i < end ){
        uint64_t timestamp;
        timestamp = *( (uint64_t *)(i) );
        i += sizeof(uint64_t);

        uint64_t blob_length;
        blob_length = *( (uint64_t *)(i) );
        i += sizeof(uint64_t);

        output.push_back(std::pair<uint64_t, std::string>(timestamp, std::string(i, blob_length)));
        i += blob_length;
    }

    return output;
}
