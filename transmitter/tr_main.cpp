#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <deque>
#include <cassert>
#include <set>

#define NDEBUG

#include "tr_structs.h"

#include <boost/program_options.hpp>
#include <stdexcept>

namespace po = boost::program_options;

uint64_t student_id = 111111;

bool begins_with(std::vector<char> packet, std::string to_be_found){
    if(to_be_found.size() > packet.size())
        return false;

    for(size_t i = 0; i < to_be_found.size(); ++i){
        if(packet[i] != to_be_found[i])
            return false;
    }

    return true;
}

void control_thread(rexmit_set_t &rexmit_set, uint16_t ctrl_port, uint64_t data_port,
                    std::string ip, std::string station_name){

    UDPMulticastReader udp_multicast_reader(ctrl_port, ip);
    UDPWriter udp_writer(ctrl_port);

    while(true){
        auto packet = udp_multicast_reader.read();
        auto& packet_data = packet.first;
        auto& packet_sender = packet.second;

        if(begins_with(packet_data, "ZERO_SEVEN_COME_IN\n")){

            std::string response;
            response  = "BOREWICZ_HERE " + ip + " ";
            response += std::to_string(data_port) + " " + station_name + "\n";

            udp_writer.write(response, packet_sender);

        } else if(begins_with(packet_data, "LOUDER_PLEASE ")){

            // bit ugly
            packet_data = std::vector<char>(packet_data.begin()+14, packet_data.end());
            for(auto id : split_ids(packet_data, ','))
                rexmit_set.add(id);

        } else {

            lck(io_mutex);
            std::cerr << "Got an unknown packet: ";
            for(unsigned char c : packet_data)
                std::cerr << (int) c << " ";
            std::cerr << "\n";
            std::cerr << std::string(packet_data.begin(), packet_data.end());
            ulck(io_mutex);

        }
    }
}

void rexmit_thread(rexmit_set_t &rexmit_set, uint64_t rtime, data_writer_t &data_queue){
    while(true){
        const auto& ids = rexmit_set.get();
        if(!ids.empty())
            data_queue.add_rexmits(ids);
        std::this_thread::sleep_for(std::chrono::milliseconds(rtime));
    }
}

int main(int argc, char* argv[]){

    // See task description for more info
    std::string mcast_addr;
    uint16_t data_port;
    uint16_t ctrl_port;
    uint64_t packet_size; // in bytes
    uint64_t rexmit_queue_size; // in bytes
    uint64_t rexmit_interval; // how often to respond to retransmit requests (in milliseconds)

    std::string station_name;
    uint64_t session_id = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                          ).count();
    
    try{
        po::options_description description("Transmitter usage");
        description.add_options()
          (",a",  po::value<std::string>(&mcast_addr)->required(),                               "MCAST_ADDR")
          (",P",  po::value<uint16_t>(&data_port)->default_value(20000 + (student_id % 10000)),  "DATA_PORT")
          (",C",  po::value<uint16_t>(&ctrl_port)->default_value(30000 + (student_id % 10000)),  "CTRL_PORT")
          (",p",  po::value<uint64_t>(&packet_size)->default_value(512),                         "PSIZE")
          (",f",  po::value<uint64_t>(&rexmit_queue_size)->default_value(128*1024),              "FSIZE")
          (",R",  po::value<uint64_t>(&rexmit_interval)->default_value(250),                     "RTIME")
          (",n",  po::value<std::string>(&station_name)->default_value("Nienazwany Nadajnik"),   "PSIZE")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, description), vm);

        po::notify(vm);
        
        if(data_port == 0)
            throw std::invalid_argument("Invalid data port");

        if(ctrl_port == 0)
            throw std::invalid_argument("Invalid ctrl port");
            
        if(station_name.size()>64)
            throw std::invalid_argument("too long station name");

    } catch(std::exception& e){
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Init the structures / other threads
    rexmit_set_t rexmit_set;
    data_writer_t data_queue(rexmit_queue_size/packet_size, data_port, mcast_addr);

    std::thread ctrl_thread(control_thread, std::ref(rexmit_set), ctrl_port, data_port, mcast_addr, station_name);
    ctrl_thread.detach();

    std::thread rxmit_thread(rexmit_thread, std::ref(rexmit_set), rexmit_interval, std::ref(data_queue));
    rxmit_thread.detach();

    // The buffer we'll be reading the data into
    char *buffer = (char*)malloc(packet_size+1);
    buffer[packet_size] = '\0';

    uint64_t byte_number = 0;
    while(true){
        size_t how_much_read_in_total = 0;
        do {
            ssize_t bytes_read = read(STDIN_FILENO, 
                                      buffer + how_much_read_in_total,
                                      packet_size - how_much_read_in_total);

            if(bytes_read == -1){
                std::cerr << "Error while reading from STDIO: " << errno << "\n";
                return 1;
            }
            
            how_much_read_in_total += bytes_read;
            
            if(bytes_read == 0 && how_much_read_in_total != packet_size)
                return 0;

            
        } while(how_much_read_in_total != packet_size);


        packet_t p(session_id, byte_number);
        p.append(std::vector<char>(buffer, buffer+packet_size));

        data_queue.add_packet(std::move(p));
        byte_number += packet_size;
    }
}
