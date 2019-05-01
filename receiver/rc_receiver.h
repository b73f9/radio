class Receiver{
    private:
        Radio *radio;
        int sock = -1;
        int port = -1;
        std::string ip;
        bool reconnect_needed = false;
        std::atomic_flag access_mutex = ATOMIC_FLAG_INIT;

        void loop(){
            while(true){

                sockaddr_in local_address;
                ip_mreq ip_mreq;

                lck(access_mutex);
                if(port == -1){
                    ulck(access_mutex);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                reconnect_needed = false;

                try{
                    sock = socket(AF_INET, SOCK_DGRAM, 0);
                    if(sock < 0)
                        throw std::runtime_error("socket creation failed");
    
                    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                    if(inet_aton(ip.c_str(), &ip_mreq.imr_multiaddr) == 0)
                        throw std::runtime_error("inet_aton failed");

                    if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof(ip_mreq)) < 0)
                        throw std::runtime_error("ip_add_membership failed");

                    int rcvbuff = 64 * 1024 * 1024;
                    if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuff, sizeof(rcvbuff)) < 0)
                        throw std::runtime_error("receive buffer size set failed");
                        
                    timeval tv;
                    tv.tv_sec = 0;
                    tv.tv_usec = 250000;
                    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
                        throw std::runtime_error("timeout set failed!");
                    
                    local_address.sin_family = AF_INET;
                    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
                    local_address.sin_port = htons(port);
                    if(bind(sock, (sockaddr *)&local_address, sizeof(local_address)) < 0)
                        throw std::runtime_error("bind failed");

                    ulck(access_mutex);

                    while(true){
                        std::vector<char> buffer(10240);
                        sockaddr_in addr = sockaddr_in();
                        socklen_t len = sizeof(addr);

                        int rcv_len = recvfrom(sock, buffer.data(), buffer.size(), 0, (sockaddr *)&addr, &len);
                        if (rcv_len <= 0 && rcv_len != ETIMEDOUT)
                            throw std::runtime_error(std::string("read failed ") + strerror(errno));

                        lck(access_mutex);
                        if(reconnect_needed){
                            reconnect_needed = false;
                            throw std::runtime_error("reconnecting");
                        }
                        ulck(access_mutex);

                        if(rcv_len != ETIMEDOUT){
                            buffer.resize(rcv_len);
                            radio->newPacket(std::move(buffer), addr);
                        }
                    }
                    
                } catch(std::runtime_error &e){
                    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof(ip_mreq));
                    shutdown(sock, SHUT_RDWR);
                    close(sock);
                    if(e.what() != std::string("read failed"))
                        ulck(access_mutex);
                }
            }
        }

    public:
        Receiver(Radio *radio){
            this->radio = radio;
            this->radio->registerReceiver(this);

            std::thread rcthread([=]{this->loop();});
            rcthread.detach();
        }

        void stopListening(){
            lck(access_mutex);
            port = -1;
            ulck(access_mutex);
        }

        void startListening(std::string ip, uint16_t port){
            lck(access_mutex);
            reconnect_needed = true;
            this->ip = ip;
            this->port = port;
            ulck(access_mutex);
        }
};
