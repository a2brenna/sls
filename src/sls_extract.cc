#include "sls.h"
#include <smpl.h>
#include <smplsocket.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::string KEY = "";
size_t LAST = 0;

void config(int argc, char *argv[]){
    po::options_description desc("Options");

    desc.add_options()
        ("help", "Produce help messagwe")
        ("key", po::value<std::string>(&KEY), "Key to retrieve values for")
        ("last", po::value<size_t>(&LAST), "Number of most recent elements to retreive")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(0);
    }

    if(KEY == ""){
        std::cout << desc << std::endl;
        exit(1);
    }

    if(LAST == 0){
        std::cout << desc << std::endl;
        exit(1);
    }

}

int main(int argc, char *argv[]){
    config(argc, argv);

    sls::global_server = std::shared_ptr<smpl::Remote_Address>(new smpl::Remote_UDS("/tmp/sls.sock"));

    const auto result = sls::lastn(KEY, LAST);

    for(const auto &r: *result){
        std::cout << r.time() << " " << r.data() << std::endl;
    }

    return 0;
}
