#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <thread>
#include <map>
#include <strings.h>
#include <queue>
#include <string>
#include <algorithm>
#include <deque>
#include <cassert>
#include <chrono>
#include <set>

#include <boost/program_options.hpp>
#include <stdexcept>

#define NDEBUG

namespace po = boost::program_options;

std::atomic_flag io_mutex = ATOMIC_FLAG_INIT;

void lck(std::atomic_flag &f){
    while(f.test_and_set(std::memory_order_acquire));
}

void ulck(std::atomic_flag &f){
    f.clear(std::memory_order_release);
}

#include "rc_radio.h"
#include "rc_telnetMgr.h"
#include "rc_receiver.h"
#include "rc_timer.h"
#include "rc_stdOutWriter.h"
#include "rc_StationList.h"
#include "rc_telnet.h"

uint64_t student_id = 111111;

int main(int argc, char *argv[]){

    std::string dscvr_addr;
    uint16_t ctrl_port;
    uint16_t ui_port;
    uint64_t buffer_size;
    uint64_t rexmit_request_interval;
    std::string station_to_wait_for;

    try{
        po::options_description description("Receiver usage");
        description.add_options()
          (",d",  po::value<std::string>(&dscvr_addr)->default_value("255.255.255.255"),         "DISCOVER_ADDR")
          (",C",  po::value<uint16_t>(&ctrl_port)->default_value(30000 + (student_id % 10000)),  "CTRL_PORT")
          (",U",  po::value<uint16_t>(&ui_port)->default_value(10000 + (student_id % 10000)),    "UI_PORT")
          (",b",  po::value<uint64_t>(&buffer_size)->default_value(65536),                       "BSIZE")
          (",R",  po::value<uint64_t>(&rexmit_request_interval)->default_value(250),             "RTIME")
          (",n",  po::value<std::string>(&station_to_wait_for)->default_value(""),               "WAIT_FOR")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, description), vm);

        po::notify(vm);
        
        if(ui_port == 0)
            throw std::invalid_argument("Invalid ui port");

        if(ctrl_port == 0)
            throw std::invalid_argument("Invalid ctrl port");

    } catch(std::exception& e){
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }


    Radio radio(buffer_size, ctrl_port);
    Timer timer(&radio, rexmit_request_interval);

    Receiver receiver(&radio);
    AnnouncementListener announcementListener(&radio, ctrl_port, dscvr_addr);

    TelnetManager telnetMgr(&radio);
    TelnetServer telnetServer(ui_port, &telnetMgr);

    StdOutWriter stdOutWriter(&radio);

    if(!station_to_wait_for.empty())
        radio.setWaitFor(station_to_wait_for);
    radio.spawnThread();

    stdOutWriter.loop();

    return 0;
}
