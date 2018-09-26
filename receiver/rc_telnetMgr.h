class client_disconnected : public std::exception{
    std::string what(){ return "Client disconnected"; }
};


class TelnetManager{
    private:

        class ConnInstance_t{
            public:
                int socket;
                int width;
                int height;
                ConnInstance_t(int Socket){
                    socket = Socket;
                    width = 80;
                    height = 20;
                }
                ConnInstance_t(){
                    socket = -1;
                }
        };

        class Message_t{
            public:
                enum class type_t{
                    InstanceReady,
                    InstanceWindowSize,
                    InstanceKillMe,
                    InstanceArrowUp,
                    InstanceArrowDown,
                    RadioNewMenu
                };
                type_t type;

                int socket;

                int width;
                int height;

                std::vector<std::string> menu;
                int current_item;
        };

        const std::string separator =
                "------------------------------------------------------------------------";

        std::atomic_flag access_mutex = ATOMIC_FLAG_INIT;

        std::map<int, ConnInstance_t> connections;
        std::queue<Message_t> message_queue;
        
        std::vector<std::string> menu;
        int current_item;

        Radio *radio;
        
        void sendLine(int client, size_t width, const std::string text){
            std::vector<unsigned char> data(text.begin(), text.end());

            for(uint32_t i=data.size();i<width;++i)
                data.push_back(' ');
            data.push_back('\r');
            data.push_back('\n');

            sendBytes(client, data);
        }

        void sendMenu(int client, int width, int height){
            try{
                sendBytes(client, {27, '[', 'H'});

                sendLine(client, width, separator);
                sendLine(client, width, "  SIK Radio");
                sendLine(client, width, separator);

                for(size_t i=0;i<menu.size();++i){
                    std::string station_entry;

                    if((int)i == current_item)
                        station_entry = "  > ";
                    else
                        station_entry = "    ";

                    station_entry += menu[i];
                    sendLine(client, width, station_entry);
                }

                sendLine(client, width, separator);

                for(int32_t i=0;i<height-5-(int)menu.size();++i)
                   sendLine(client, width, "");

            } catch(client_disconnected &e){}
        }

        void sendBytes(int client, const std::vector<unsigned char> &msg){
            int snd_len = write(client, &msg[0], msg.size());
            if(snd_len != (int)msg.size()) {
                lck(io_mutex);
                std::cerr << "Write failed\n";
                ulck(io_mutex);

                throw client_disconnected();
            }
        }

        void broadcast_menu(){
            for(auto x : connections){
                const auto& instance = x.second;
                sendMenu(instance.socket, instance.width, instance.height);
            }
        }
        
        void loop(){
            while(true){
                lck(access_mutex);

                if(message_queue.empty()){
                    ulck(access_mutex);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                Message_t msg = message_queue.front();
                message_queue.pop();

                ulck(access_mutex);

                if(msg.type == Message_t::type_t::InstanceArrowUp)
                    radio->instanceArrowUp();

                if(msg.type == Message_t::type_t::InstanceArrowDown)
                    radio->instanceArrowDown();

                if(msg.type == Message_t::type_t::InstanceKillMe)
                    connections.erase(msg.socket);

                if(msg.type == Message_t::type_t::InstanceReady){
                    connections[msg.socket] = ConnInstance_t(msg.socket);
                    sendMenu(msg.socket, 80, 20);
                }

                if(msg.type == Message_t::type_t::InstanceWindowSize){
                    connections[msg.socket].width = msg.width;
                    connections[msg.socket].height = msg.height;
                    sendMenu(msg.socket, msg.width, msg.height);
                }
                
                if(msg.type == Message_t::type_t::RadioNewMenu){
                    menu = msg.menu;
                    current_item = msg.current_item;
                    broadcast_menu();
                }
            }
        }

        void queueMessage(Message_t &&msg){
            lck(access_mutex);
            message_queue.push(std::move(msg));
            ulck(access_mutex);
        }

    public:
        TelnetManager(Radio *radio){
            this->radio = radio;
            this->radio->registerMgr(this);

            std::thread mgrthread([=]{this->loop();});
            mgrthread.detach();
        }

        // Events
        void instanceReady(int socket){
            Message_t msg;
            msg.type = Message_t::type_t::InstanceReady;
            msg.socket = socket;
            queueMessage(std::move(msg));
        }

        void instanceKillMe(int socket){
            Message_t msg;
            msg.type = Message_t::type_t::InstanceKillMe;
            msg.socket = socket;
            queueMessage(std::move(msg));
        }

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

        void instanceWindowSize(int socket, int height, int width){
            Message_t msg;
            msg.type = Message_t::type_t::InstanceWindowSize;
            msg.socket = socket;
            msg.width = width;
            msg.height = height;
            queueMessage(std::move(msg));
        }
        
        void radioNewMenu(std::vector<std::string> menu, int current_position){
            Message_t msg;
            msg.type = Message_t::type_t::RadioNewMenu;
            msg.menu = menu;
            msg.current_item = current_position;
            queueMessage(std::move(msg));
        }

};


