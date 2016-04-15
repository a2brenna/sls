#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>
#include <memory>
#include <slog/slog.h>

extern std::string CONFIG_DISK_DIR;
extern int CONFIG_PORT;
extern std::string CONFIG_UNIX_DOMAIN_FILE;
extern size_t CONFIG_RESOLUTION;
extern size_t CONFIG_NUM_BUCKETS;
extern std::unique_ptr<slog::Log> Error;
extern std::unique_ptr<slog::Log> Info;

void get_config(int argc, char *argv[]);

#endif
