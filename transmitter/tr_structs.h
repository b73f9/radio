#include "tr_udp.h"
#include "tr_misc.h"

auto uint64ToByteVector(uint64_t data){
    auto data_vector = std::vector<char>((char*)&data, (char*)&data+8);
    std::reverse(begin(data_vector), end(data_vector));

    return data_vector;
}

class packet_t{
    private:
        uint64_t packet_id;
        std::vector<char> data;

    public:
        packet_t(uint64_t session_id, uint64_t packet_id){
            this->packet_id = packet_id;
            this->append(uint64ToByteVector(session_id));
            this->append(uint64ToByteVector(packet_id));
        }

        void append(const std::vector<char> &str){
            for(auto s : str)
                data.push_back(s);
        }

        uint64_t get_id(){
            return packet_id;
        }

        const std::vector<char>& get_data(){
            return data;
        }
};


class rexmit_set_t {
    private:
        std::atomic_flag m = ATOMIC_FLAG_INIT;
        std::set<uint64_t> ids;
        std::set<uint64_t> old_ids;

    public:
        void add(uint64_t id){
            lck(m);
            ids.insert(id);
            ulck(m);
        }

        const std::set<uint64_t>& get(){
            lck(m);
            std::swap(ids, old_ids);
            ids.clear();
            ulck(m);

            return old_ids; 
        }
};

class data_writer_t {
    private:
        uint64_t queue_size;
        std::atomic_flag m = ATOMIC_FLAG_INIT;
        UDPMulticastWriter multicast_writer;

        std::deque<packet_t> avialable_data;
        std::deque<uint64_t> ids_to_send;

        void send_packet(uint64_t id){
            if(avialable_data.front().get_id() < id || avialable_data.back().get_id() > id)
                return;

            for(auto p : avialable_data){
                if(p.get_id() == id){
                    multicast_writer.write(p.get_data());
                    break;
                }
            }
        }

    public:
        data_writer_t(uint64_t Qsize, uint64_t Port, std::string Ip){
            queue_size = Qsize;
            multicast_writer = UDPMulticastWriter(Port, Ip);
        }

        void add_packet(packet_t &&packet){
            lck(m);

            uint64_t id = packet.get_id();
            avialable_data.push_front(std::move(packet));

            if(avialable_data.size()>queue_size)
                avialable_data.pop_back();

            send_packet(id);

            ulck(m);
        }

        void add_rexmits(const std::set<uint64_t> &ids){
            lck(m);
            for(auto id : ids)
                send_packet(id);
            ulck(m);
        }
};
