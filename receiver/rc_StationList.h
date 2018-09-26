class AnnouncementListener{
    private:
        Radio *radio;
        std::atomic_flag access_mutex = ATOMIC_FLAG_INIT;
        std::string multicast_addr;
        uint16_t port;

        // name, broadcast ip, port
        typedef std::tuple<std::string, std::string, uint16_t> StationKey_t;

        // StationKey_t -> (last announce, ip)
        std::map<StationKey_t,
                 std::pair<
                           std::chrono::system_clock::time_point,
                           sockaddr_in
                          >
                > stations;

        std::string getTillNextSpace(std::vector<char> &pckt){
            auto found_iter = std::find_if(pckt.begin(), pckt.end(),
                                           [](char c){return c == ' ' || c == '\n';});

            std::string result = std::string(pckt.begin(), found_iter);
            if(found_iter != pckt.end())
                ++found_iter; // ignore the delimeter, if it was found

            pckt = std::vector<char>(found_iter, pckt.end());
            return result;
        }

        void emit_stations(){
            std::vector<StationInfo_t> st;
            for(auto s : stations){
                // bit ugly
                StationInfo_t info;
                std::get<0>(info) = std::get<0>(s.first);
                std::get<1>(info) = std::get<1>(s.first);
                std::get<2>(info) = s.second.second;
                std::get<3>(info) = std::get<2>(s.first);
                st.push_back(info);
            }

            radio->stationList(std::move(st));
        }
        
        void listen_loop(){

            UDPReader reader(port);
            while(true){

                auto packet = reader.read();
                auto& packet_data = packet.first;
                auto& packet_source = packet.second;

                const std::string type = getTillNextSpace(packet_data);
                if(type == "BOREWICZ_HERE"){

                    const std::string ip = getTillNextSpace(packet_data);

                    uint16_t port;
                    try {
                        port = stoi(getTillNextSpace(packet_data));
                    } catch(std::exception &e){
                        lck(io_mutex);
                        std::cerr << "Got a malformed packet, stoi threw an exception:\n";
                        std::cerr << e.what() << "\n";
                        ulck(io_mutex);

                        continue;
                    }

                    if(port == 0 || packet_data.size()==0){
                        lck(io_mutex);
                        std::cerr << "Got a malformed packet!\n";
                        ulck(io_mutex);

                        continue;
                    }

                    std::string name = std::string(packet_data.begin(), packet_data.end()-1);
                    StationKey_t key = {name, ip, port};

                    lck(access_mutex);
                    stations[key] = {std::chrono::system_clock::now(), packet_source};
                    ulck(access_mutex);

                    emit_stations();

                } else if(type != "ZERO_SEVEN_COME_IN"){
                    lck(io_mutex);
                    std::cerr << "Got an unknown packet: " << type + " " + std::string(packet_data.begin(), packet_data.end()) << "\n";
                    ulck(io_mutex);
                }
            }
        }

        void request_loop(){

            UDPMulticastWriter writer(port, multicast_addr);
            while(true){
                writer.write("ZERO_SEVEN_COME_IN\n");

                lck(access_mutex);
                bool changed = false;
                for(auto it = stations.begin();it!=stations.end();){

                    auto elapsed = std::chrono::system_clock::now() - it->second.first;
                    if(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 20){
                        stations.erase(it++);
                        changed = true;
                    } else
                        it++;

                }

                if(changed)
                    emit_stations();
                ulck(access_mutex);

                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }

    public:
        AnnouncementListener(Radio *radio, uint16_t port, std::string multicast_addr){
            assert(radio != nullptr);

            this->radio = radio;
            this->port = port;
            this->multicast_addr = multicast_addr;

            std::thread listener_thread([=]{this->listen_loop();});
            listener_thread.detach();

            std::thread requester_thread([=]{this->request_loop();});
            requester_thread.detach();
        }
};
