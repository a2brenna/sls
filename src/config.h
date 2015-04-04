#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

extern unsigned long cache_min;
extern unsigned long cache_max;
extern std::string disk_dir;
extern int port;
extern std::string CONFIG_UNIX_DOMAIN_FILE;

void get_config(int argc, char *argv[]);

#endif
