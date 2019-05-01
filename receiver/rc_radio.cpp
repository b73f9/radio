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
#include <set>

extern std::atomic_flag io_mutex;
void lck(std::atomic_flag &f);
void ulck(std::atomic_flag &f);

#include "rc_radio.h"
#include "rc_telnetMgr.h"
#include "rc_receiver.h"
#include "rc_timer.h"
#include "rc_stdOutWriter.h"

void Radio::emit_menu(){
    std::vector<std::string> station_names;

    for(auto station : stations)
        station_names.push_back(std::get<0>(station));

    mgr->radioNewMenu(station_names, getCurrentStationId(stations));
}

void Radio::startListening(StationInfo_t station){
    buffer.clear();
    noStation = false;

    current_station = station;
    std::get<2>(current_station).sin_port = htons(ctrlport);
    rcvr->startListening(std::get<1>(station), std::get<3>(station));
}

void Radio::stopListening(){
    buffer.clear();
    noStation = true;

    current_session_id = 0;
    rcvr->stopListening();
}


void Radio::loop(){
    while(true){
        lck(m);

        if(message_queue.empty()){
            ulck(m);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        Message_t msg = message_queue.front();
        message_queue.pop();

        ulck(m);
        
        if(msg.type == Message_t::type_t::InstanceArrowUp)
            instanceArrowUpHandler();

        if(msg.type == Message_t::type_t::InstanceArrowDown)
            instanceArrowDownHandler();
            
        if(msg.type == Message_t::type_t::StationList)
            stationListHandler(msg);
        
        if(msg.type == Message_t::type_t::ReceiveGotPacket)
            receiveGotPacketHandler(msg);

        if(msg.type == Message_t::type_t::RexmitReminder)
            rexmitReminderHandler(msg);
    }
}

void Radio::instanceArrowUpHandler(){
    assert(mgr != nullptr);

    int id = getCurrentStationId(stations);
    if(id > 0)
        switchToStation(stations[id-1]);
}

void Radio::instanceArrowDownHandler(){
    assert(mgr != nullptr);

    int id = getCurrentStationId(stations);
    if(id < (int)stations.size()-1 && id >= 0)
        switchToStation(stations[id+1]);
}

void Radio::stationListHandler(Message_t msg){
    stations = std::move(msg.list);
    std::sort(stations.begin(), stations.end(),
              [](const auto &l, const auto &r){return std::get<0>(l) < std::get<0>(r);});

    int id = getCurrentStationId(stations);
    if(id == -1 && !noStation){
        stopListening();
    }

    if(!waitForSpecificStation && noStation && stations.size()){
        startListening(stations[0]);
    }

    if(waitForSpecificStation && noStation && stations.size()){
        for(auto st : stations)
            if(std::get<0>(st) == nameOfTheStationWaitingFor){
                startListening(st);
                waitForSpecificStation = false;
                nameOfTheStationWaitingFor = "";
                break;
            }
    }

    emit_menu();
}

void Radio::receiveGotPacketHandler(Message_t msg){
    if(std::string(inet_ntoa(msg.source.sin_addr)) != std::string(inet_ntoa(std::get<2>(current_station).sin_addr)))
        return;

    if(msg.data.size() <= 16){
        lck(io_mutex);
        std::cerr << "WRONG PACKET SIZE " << msg.data.size() << "\n";
        ulck(io_mutex);
    }

    uint64_t session_id = vectorToUint64(msg.data.begin());
    uint64_t chunk_id   = vectorToUint64(msg.data.begin()+8);

    if(session_id < current_session_id)
        return;

    if(current_session_id == 0)
        current_session_id = session_id;

    if(current_session_id < session_id){
        current_session_id = session_id;
        buffer.clear();
    }

    uint64_t packet_size = msg.data.size()-16;
    uint64_t lastchunkid = buffer.add(chunk_id, msg.data);

    if(lastchunkid != chunk_id && lastchunkid < chunk_id - packet_size){
        // add rexmit reminder for {lastchunkid + chunk_size, ... , curpos - chunk_size}
        std::vector<uint64_t> temp;
        for(uint i=lastchunkid+packet_size;i<chunk_id;i+=packet_size)
            temp.push_back(i);
        timer->setReminder(temp, current_session_id);
    }

}

void Radio::rexmitReminderHandler(Message_t msg){
    if(msg.sess != current_session_id)
        return;

    std::vector<uint64_t> torexmit;
    for(auto c : msg.chunkids)
        if(buffer.isRexmittable(c))
            torexmit.push_back(c);

    if(torexmit.size()){
        timer->setReminder(torexmit, current_session_id);

        std::string msg = "";
        msg += "LOUDER_PLEASE ";
        msg += std::to_string(torexmit[0]);
        for(size_t i=1;i<torexmit.size();++i){
            msg += ",";
            msg += std::to_string(torexmit[i]);
        }
        msg += "\n";

        sckt.write(msg, std::get<2>(current_station));
    }
}
