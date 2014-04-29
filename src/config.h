#ifndef __CONFIG_H__
#define __CONFIG_H__ 1

#include <string>

unsigned int cache_min = 3600 * 2;
unsigned int cache_max = 3600 * 4;
std::string disk_dir = "/pool/sls/";
int port = 6998;
std::string unix_domain_file = "/tmp/sls.sock";

#endif
