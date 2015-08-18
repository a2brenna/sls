#include "sls.h"
#include <smpl.h>
#include <smplsocket.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::string KEY = "";
bool ALL = false;
size_t LAST = 0;
size_t INDEX_START = 0;
size_t INDEX_END = 0;
uint64_t TIME_START = 0;
uint64_t TIME_END = 0;

void config(int argc, char *argv[]){
    po::options_description desc("Options");

    desc.add_options()
        ("help", "Produce help messagwe")
        ("key", po::value<std::string>(&KEY), "Key to retrieve values for")
        ("last", po::value<size_t>(&LAST), "Number of most recent elements to retrieve")
        ("start", po::value<uint64_t>(&TIME_START), "Time to start retrieving elements")
        ("end", po::value<uint64_t>(&TIME_END), "Time to start retrieving elements")
        ("all", po::bool_switch(&ALL), "Retrieve all elements")
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

}

int main(int argc, char *argv[]){
    config(argc, argv);

    sls::global_server = std::shared_ptr<smpl::Remote_Address>(new smpl::Remote_UDS("/tmp/sls.sock"));

    std::shared_ptr<std::deque<sls::Value>> result;
    if(LAST > 0){
        result = sls::lastn(KEY, LAST);
    }
    else if(TIME_END > TIME_START){
        result = sls::intervalt(KEY, TIME_START, TIME_END);
    }
    else if(ALL){
        result = sls::all(KEY);
    }
    else{
        return -1;
    }

    for(const auto &r: *result){
        std::cout << r.time() << " " << r.data() << std::endl;
    }

    return 0;
}
