class TelnetInstance{
    private:

        TelnetManager *telnetMgr;
        int socket;

        uint16_t width = 80;
        uint16_t height = 20;

        char buffer[1024];
        char *next_byte;
        int32_t remaining_bytes;

        unsigned char get_byte(){
            if(remaining_bytes == 0){

                remaining_bytes = read(socket, buffer, sizeof(buffer));

                if(remaining_bytes <= 0){
                    if(remaining_bytes < 0){
                        lck(io_mutex);
                        std::cerr << "Read failed\n";
                        ulck(io_mutex);
                    }

                    throw client_disconnected();
                }

                next_byte = buffer;
            }

            --remaining_bytes;
            return *(next_byte++);
        }
        
        std::vector<unsigned char> get_bytes(uint32_t count){
            std::vector<unsigned char> c;
            for(uint32_t i=0;i<count;++i)
                c.push_back(get_byte());
            return c;
        }

        void send_bytes(const std::vector<unsigned char> &msg){
            int snd_len = write(socket, &msg[0], msg.size());
            if(snd_len != (int)msg.size()) {

                lck(io_mutex);
                std::cerr << "write failed\n";
                ulck(io_mutex);

                throw client_disconnected();
            }
        }

        // ==== Telnet protocol message handlers =============================

        void handle_IAC_WILL(){
            const unsigned char byte = get_byte();
            // Unless SUPPRESS-GO-AHEAD, NAWS OR LINEMODE
            if(byte != 3 && byte != 31 && byte != 34){
                lck(io_mutex);
                std::cerr << "Received unknown IAC WILL " << (int)byte << "\n";
                ulck(io_mutex);
            }
        }

        void handle_IAC_WONT(){
            const unsigned char byte = get_byte();
            if(byte != 34){
                lck(io_mutex);
                std::cerr << "Received IAC WONT " << (int) byte << "\n";
                ulck(io_mutex);
            }
        }

        void handle_IAC_DO(){
            const unsigned char byte = get_byte();
            // Unless SUPPRESS-GO-AHEAD or ECHO
            if(byte != 3 && byte != 1) {
                lck(io_mutex);
                std::cerr << "Received unknown IAC DO " << (int) byte << "\n";
                ulck(io_mutex);
            }
        }

        void handle_IAC_SB(){
            const unsigned char byte = get_byte();
            if(byte == 31){ // IAC SB NAWS
                height = (get_byte() << 8) + get_byte();
                width  = (get_byte() << 8) + get_byte();
                if(height > 10000 || width > 10000){
                    width = 80;
                    height = 20;

                    lck(io_mutex);
                    std::cerr << "Invalid data while negotiating window size!\n";
                    ulck(io_mutex);
                }

                telnetMgr->instanceWindowSize(socket, width, height);
            } else if(byte != 34){ // unless IAC SB LINEMODE
                lck(io_mutex);
                std::cerr << "Received unknown IAC SB " << (int) byte << "\n";
                ulck(io_mutex);
            }

            while(get_byte() != 255);
            if(get_byte() != 240){
                lck(io_mutex);
                std::cerr << "Invalid data while parsing IAC SB " << byte << "!\n";
                ulck(io_mutex);
            }
        }


        void handle_escape_sequence(){
            if(get_byte() == '['){
                const unsigned char byte = get_byte();
                if(byte == 65){ // Arrow up
                    telnetMgr->instanceArrowUp();
                }
                if(byte == 66){ // Arrow down
                    telnetMgr->instanceArrowDown();
                }
            }
        }

    public:
        TelnetInstance(int Socket, TelnetManager *Mgr){
            socket = Socket;
            remaining_bytes = 0;
            telnetMgr = Mgr;
        }

        void clientHandler(){
            try{
                // Negotiate NAWS
                send_bytes({255, 253, 31});

                // Suppress GO-AHEAD
                send_bytes({255, 253, 3});

                // Disable LINEMODE
                send_bytes({255, 253, 34});
                send_bytes({255, 250, 34, 1, 0, 255, 240});

                // Enable remote echo
                send_bytes({255, 251, 1});

                telnetMgr->instanceReady(socket);

                while(true){
                    unsigned char byte = get_byte();
                    if(byte == 255){ // IAC
                        byte = get_byte();
                        if(byte == 251){ // WILL
                            handle_IAC_WILL();
                        } else if(byte == 252){ // WONT
                            handle_IAC_WONT();
                        } else if(byte == 253) { // DO
                            handle_IAC_DO();
                        } else if(byte == 250) { // SB
                            handle_IAC_SB();
                        } else {
                            lck(io_mutex);
                            std::cerr << "Received unknown IAC " << (int) byte << "\n";
                            ulck(io_mutex);
                        }
                    } else if(byte == 27) { // Received escape sequence
                        handle_escape_sequence();
                    }
                }
            } catch(client_disconnected &e){
                telnetMgr->instanceKillMe(socket);
            }
        }

        ~TelnetInstance(){
            close(socket);
        }
};

class TelnetServer {
    private:
        int sock;
        int port;
        struct sockaddr_in server_addr;

        bool ready = false;
        TelnetManager *mgr;

        void deinit(){
            close(sock);
        }

        void clientHandler(int client){
            TelnetInstance t(client, mgr);
            t.clientHandler(); // returns after disconnect
        }

        void loop(){
            while(true){
                try{
                    sock = socket(AF_INET, SOCK_STREAM, 0);
                    if(sock < 0)
                        throw std::runtime_error("Socket creation failed!");
                    
                    int tr = 1;
                    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int)) < 0)
                        throw std::runtime_error("SetSockOpt failed!");

                    server_addr.sin_family = AF_INET;
                    server_addr.sin_port = htons(port);
                    server_addr.sin_addr.s_addr = INADDR_ANY;
                    bzero(&(server_addr.sin_zero), 8);

                    if(bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0)
                        throw std::runtime_error("Bind failed!");

                    if(listen(sock, 64) < 0)
                        throw std::runtime_error("Listen failed!");

                    while(true){
                        int client = accept(sock, nullptr, nullptr);
                        if(client < 0)
                            throw std::runtime_error("Accept failed!");

                        std::thread client_thread([=]{this->clientHandler(client);});
                        client_thread.detach();
                    }

                } catch(std::runtime_error &e){
                    deinit();

                    lck(io_mutex);
                    std::cerr << "Error: " << e.what() << " #" << errno << "\n"; 
                    std::cerr << "Retrying in 3 seconds...\n";
                    ulck(io_mutex);

                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
        }

    public:
        TelnetServer(int port, TelnetManager *telnetMgr){
            this->port = port;
            this->mgr  = telnetMgr;

            std::thread srvthread([=]{this->loop();});
            srvthread.detach();
        }
};
