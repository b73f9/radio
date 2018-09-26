#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class UDPMulticastWriter {
    private:
        int sock;
        in_port_t port;
        std::string ip;

        bool ready = false;
        struct sockaddr_in remote_address;
        
        void init(){
            while(true){
                try{
                    memset(&remote_address, 0, sizeof(struct sockaddr_in));
                    sock = socket(AF_INET, SOCK_DGRAM, 0);
                    if(sock < 0)
                        throw std::runtime_error("Socket creation failed!");

                    int optval = 1;
                    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
                        throw std::runtime_error("setsockopt broadcast");

                    optval = 4; // ttl
                    if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
                        throw std::runtime_error("setsockopt multicast ttl");

                    // Set buffer size
                    int sendbuff = 1024*1024;

                    if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)) < 0)
                        throw std::runtime_error("SNDBUF set failed!");
                    
                    
                    remote_address.sin_family = AF_INET;
                    remote_address.sin_port = htons(port);
                    if(inet_aton(ip.c_str(), &remote_address.sin_addr) == 0)
                        throw std::runtime_error("inet_aton");
                    break;
                }catch(std::runtime_error& e){
                    std::cerr << "Error: " << e.what() << ", waiting 3s and retrying\n";
                    deinit();
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
            ready = true;
        }

        void deinit(){
            close(sock);
            ready = false;
        }
    public:
        UDPMulticastWriter(){}
        UDPMulticastWriter(int port, std::string ip){
            this->port = (in_port_t)port;
            this->ip = ip;
            init();
        }

        ~UDPMulticastWriter(){
            deinit();
        }

        void write(const std::vector<char> &packet){
            assert(ready);
            if(sendto(sock, packet.data(), packet.size(), 0, (struct sockaddr *) &remote_address, sizeof(remote_address))
                    != (ssize_t) packet.size()){
                std::cerr << "sendto " << errno << "\n";
                deinit();
                init();
            }
        }

        void write(const std::string &packet){
            std::vector<char> vec(packet.begin(), packet.end());
            this->write(vec);
        }
};

class UDPMulticastReader {
    private:
        int sock;
        bool ready = false;
        in_port_t port;
        std::string ip;
        bool listen;
        struct ip_mreq ip_mreq;

        void init(){
            while(true){
                try{
                    struct sockaddr_in local_address;
                    sock = socket(AF_INET, SOCK_DGRAM, 0);
                    if(sock < 0)
                        throw std::runtime_error("Socket creation failed!");
                    
                    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                    if(inet_aton(ip.c_str(), &ip_mreq.imr_multiaddr) == 0)
                        throw std::runtime_error("inet_aton failed!");

                    if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
                        throw std::runtime_error("setsockopt failed!");


                    local_address.sin_family = AF_INET;
                    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
                    local_address.sin_port = htons(port);
                    if(bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
                        throw std::runtime_error("bind failed!");
                    break;
                }catch(std::runtime_error& e){
                    std::cerr << "Error: " << e.what() << ", waiting 3s and retrying\n";
                    deinit();
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
            ready = true;
        }

        void deinit(){
            if(setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
                std::cerr << "IP_DROP_MEMBERSHIP failed!\n";
            close(sock);
            ready = false;
        }

    public:
        UDPMulticastReader(){}
        UDPMulticastReader(int Port, std::string Ip){
            port = (in_port_t)Port;
            ip = Ip;
            init();
        }
        ~UDPMulticastReader(){
            deinit();
        }
        std::pair<std::vector<char>, sockaddr_in> read(){
            assert(ready);
            sockaddr_in saddr;

            memset(&saddr, 0, sizeof(struct sockaddr_in));

            socklen_t socklen = sizeof(saddr);
            std::vector<char> buffer;
            buffer.resize(10240);
            int status = 0;
            while(true){
                try{
                    status = recvfrom(sock, buffer.data(), 10240, 0, (struct sockaddr *)&saddr, &socklen);
                    if(status < 0)
                        throw std::runtime_error("recvfrom failed!");
                    break;
                }catch(std::runtime_error& e){
                    std::cerr << "Error: " << e.what() << ", waiting 3s and retrying\n";
                    deinit();
                    init();
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }

            buffer.resize(status);
            return {buffer, saddr};
        }
};

class UDPWriter {
    private:
        bool ready = false;
        in_port_t port;
        int sock;
        struct sockaddr_in remote_address;
        
        void init(){
            while(true){
                try{
                    sock = socket(AF_INET, SOCK_DGRAM, 0);
                    if(sock < 0)
                        throw std::runtime_error("Socket creation failed");
                    break;
                }catch(std::runtime_error& e){
                    std::cerr << "Error: " << e.what() << ", waiting 3s and retrying\n";
                    deinit();
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
            ready = true;
        }

        void deinit(){
            close(sock);
            ready = false;
        }

    public:
        UDPWriter(){}
        UDPWriter(int Port){
            port = (in_port_t)Port;
            init();
        }
        ~UDPWriter(){
            deinit();
        }
        void write(const std::vector<char> &packet, sockaddr_in addr){
            assert(ready);
            addr.sin_port = htons(port);
            ssize_t send_count =
                    sendto(sock, packet.data(), packet.size(), 0, (struct sockaddr *) &addr, sizeof(addr));
            if(send_count != (ssize_t) packet.size()){
                std::cerr << "sendto " << errno << "\n";
                deinit();
                init();
            }
        }
        void write(const std::string &packet, sockaddr_in addr){
            std::vector<char> vec(packet.begin(), packet.end());
            this->write(vec, addr);
        }
};
