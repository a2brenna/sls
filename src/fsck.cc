#include "index.h"
#include <boost/program_options.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <regex>

std::string CONFIG_SLS_DIR;
std::string CONFIG_INCLUDE_REGEX = ".*";
std::string CONFIG_EXCLUDE_REGEX = "";
size_t CONFIG_RESOLUTION = 1000;

namespace po = boost::program_options;

void config(int argc, char *argv[]) {

  po::options_description desc("Options");

  desc.add_options()("help", "Produce help message")
      ("sls_dir", po::value<std::string>(&CONFIG_SLS_DIR), "Directory to database from")
      ("include_regex", po::value<std::string>(&CONFIG_INCLUDE_REGEX), "Fsck keys only if they match this regex and are not caught by the exclude_regex")
      ("exclude_regex", po::value<std::string>(&CONFIG_EXCLUDE_REGEX), "Exclude keys matching this regex")
      ("resolution", po::value<size_t>(&CONFIG_RESOLUTION), "Resolution of the index")
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    exit(0);
  }

  if (CONFIG_SLS_DIR.empty()) {
    std::cerr << "You must specify an sls directory tree to operate on"
              << std::endl;
    std::cerr << desc << std::endl;
    exit(1);
  }

}

int main(int argc, char *argv[]) {
  config(argc, argv);
    std::regex include_regex(CONFIG_INCLUDE_REGEX);
    std::regex exclude_regex(CONFIG_EXCLUDE_REGEX);

  std::vector<std::string> keys;
  const auto m = getdir(CONFIG_SLS_DIR, keys);
  if (m != 0) {
    std::cerr << "Fatal error reading from: " << CONFIG_SLS_DIR << std::endl;
    exit(1);
  }

  for (const auto &k : keys) {

    if (!std::regex_match(k, include_regex)) {
      continue;
    } else if (std::regex_match(k, exclude_regex)) {
      continue;
    }

    Path key_directory(CONFIG_SLS_DIR + "/" + k);
    Index index;
    try {
      index = build_index(key_directory.str(), CONFIG_RESOLUTION);
    } catch (...) {
      std::cerr << "Error, failure to build_index for: " << key_directory.str()
                << std::endl;
      continue;
    }

    Path index_file(CONFIG_SLS_DIR + "/" + k + "/index");
    std::ofstream o(index_file.str(),
                    std::ofstream::out | std::ofstream::trunc);
    if (!o) {
      std::cerr << "Error, unable to open file for writing: "
                << index_file.str() << std::endl;
      continue;
    } else {
      o << index;
      o.close();
      std::cout << "Indexed " << key_directory.str() << std::endl;
    }
  }

  return 0;
}
