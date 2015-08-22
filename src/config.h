#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

extern std::string CONFIG_DISK_DIR;
extern int port;
extern std::string CONFIG_UNIX_DOMAIN_FILE;
extern size_t CONFIG_RESOLUTION;

void get_config(int argc, char *argv[]);

#endif
