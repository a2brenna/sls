#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

extern unsigned long CONFIG_CACHE_MIN;
extern unsigned long CONFIG_CACHE_MAX;
extern std::string CONFIG_DISK_DIR;
extern int port;
extern std::string CONFIG_UNIX_DOMAIN_FILE;

void get_config(int argc, char *argv[]);

#endif
