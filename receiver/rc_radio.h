class TelnetManager;
class Receiver;
class Timer;
class StdOutWriter;

#include <atomic>

#include "rc_udp.h"
#include "rc_buffer.h"

typedef std::tuple<std::string, std::string, sockaddr_in, uint16_t> StationInfo_t;

class Radio{
    private:
        class Message_t{
            public:
                enum class type_t{
                    InstanceArrowUp, 
                    InstanceArrowDown,
                    StationList,
                    ReceiveGotPacket,
                    StdOutReady,
                    RexmitReminder
                };

                type_t type;

                std::vector<char> data;
                sockaddr_in source;

                std::vector<uint64_t> chunkids;
                uint64_t sess;

                std::vector<StationInfo_t> list;
        };

        TelnetManager *mgr = nullptr;
        Receiver *rcvr = nullptr;
        Timer *timer = nullptr;
        StdOutWriter *outputter = nullptr;

        Buffer_t buffer;

        UDPWriter sckt;
        uint16_t ctrlport;

        std::queue<Message_t> message_queue;
        std::atomic_flag m = ATOMIC_FLAG_INIT;
        uint64_t current_session_id = 0;

        std::vector<StationInfo_t> stations;
        bool noStation = true;
        StationInfo_t current_station = {"", "", sockaddr_in(), 0};

        bool waitForSpecificStation = false;
        std::string nameOfTheStationWaitingFor = "";
        
        void loop();
        void startListening(StationInfo_t station);
        void stopListening();

        uint64_t vectorToUint64(std::vector<char>::iterator it){
            std::vector<char> vecData(it, it+8);
            std::reverse(vecData.begin(), vecData.end());
            return *((uint64_t *)vecData.data());
        }
        
        bool StationInfoCmp(const StationInfo_t &l, const StationInfo_t &r){
            if(std::get<0>(l) != std::get<0>(r))
                return false;
            if(std::get<1>(l) != std::get<1>(r))
                return false;
            if(std::get<3>(l) != std::get<3>(r))
                return false;
            return true;
        }
        
        int getCurrentStationId(std::vector<StationInfo_t> &info){
            int item_id = -1;
            for(size_t i=0;i<info.size();++i){
                if(StationInfoCmp(info[i], current_station)){
                    item_id = i;
                    break;
                }
            }
            return item_id;
        }

        void queueMessage(Message_t &&msg){
            lck(m);
            message_queue.push(std::move(msg));
            ulck(m);
        }

        void switchToStation(const StationInfo_t station){
            stopListening();
            startListening(station);
            emit_menu();
        }

        void emit_menu();

        //Event handlers
        void instanceArrowUpHandler();
        void instanceArrowDownHandler();
        void stationListHandler(Message_t msg);
        void receiveGotPacketHandler(Message_t msg);
        void rexmitReminderHandler(Message_t msg);

    public:

        Radio(uint64_t bufsize, uint64_t Ctrlport) : buffer(bufsize), ctrlport(Ctrlport){
        }

        void spawnThread(){
            assert(mgr != nullptr);
            assert(rcvr != nullptr);
            assert(timer != nullptr);
            assert(outputter != nullptr);

            std::thread rdthread([=]{this->loop();});
            rdthread.detach();
        }

        void setWaitFor(std::string name){
            nameOfTheStationWaitingFor = name;
            waitForSpecificStation = true;
        }

        void registerMgr(TelnetManager *Mgr){
            assert(mgr == nullptr);
            mgr = Mgr;
        }

        void registerReceiver(Receiver *Rcvr){
            assert(rcvr == nullptr);
            rcvr = Rcvr;
        }

        void registerTimer(Timer *Tmer){
            assert(timer == nullptr);
            timer = Tmer;
        }

        void registerOutputter(StdOutWriter *sout){
            assert(outputter == nullptr);
            outputter = sout;
        }

        std::vector<char> getData(){
            return buffer.getPacket();
        }

        // Events
        void instanceArrowUp(){
            Message_t msg;
            msg.type = Message_t::type_t::InstanceArrowUp;
            queueMessage(std::move(msg));
        }

        void instanceArrowDown(){
            Message_t msg;
            msg.type = Message_t::type_t::InstanceArrowDown;
            queueMessage(std::move(msg));
        }

        void stationList(std::vector<StationInfo_t> &&list){
            Message_t msg;
            msg.type = Message_t::type_t::StationList;
            msg.list = std::move(list);
            queueMessage(std::move(msg));
        }

        void newPacket(std::vector<char> &&buffer, sockaddr_in addr){
            Message_t msg;
            msg.type = Message_t::type_t::ReceiveGotPacket;
            msg.data = std::move(buffer);
            msg.source = addr;
            queueMessage(std::move(msg));
        }

        void rexmitReminder(std::vector<uint64_t> chunkids, uint64_t sessid){
            Message_t msg;
            msg.type = Message_t::type_t::RexmitReminder;
            msg.chunkids = std::move(chunkids);
            msg.sess = sessid;
            queueMessage(std::move(msg));
        }
};
