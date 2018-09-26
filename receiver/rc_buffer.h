class Buffer_t{
        uint64_t size;
        uint64_t chunk_size = 0;
        uint64_t max_chunk_id = 0;
        int64_t first_chunk_id = -1;

        std::deque<std::pair<bool, std::vector<char>>> packet_queue;

        void _add(std::pair<bool, std::vector<char>> arg){
            // buffer overrun, discarding data!
            if(packet_queue.size() && packet_queue.size()*packet_queue.back().second.size() >= size){
                packet_queue.pop_front();
            }
            packet_queue.push_back(arg);
        }

    public:
        bool buffered_enough;

        Buffer_t(){}

        Buffer_t(uint64_t Size){
            size = Size;
            clear();
        }

        uint64_t add(uint64_t chunk_id, const std::vector<char> &d){
            if((int64_t)chunk_id < first_chunk_id)
                return max_chunk_id;

            if(first_chunk_id == -1){
                first_chunk_id = chunk_id;
                max_chunk_id = chunk_id;
            }

            if(std::max(max_chunk_id, chunk_id) - first_chunk_id >= 0.75 * size)
                buffered_enough = true;

            chunk_size = d.size()-16;
            int64_t cid = chunk_id / chunk_size;
            int64_t mid = max_chunk_id / chunk_size;
            int64_t dist = mid - cid;

            if(dist < 0){
                for(;dist<-1;++dist)
                    _add({false, std::vector<char>()});
                _add({true, std::vector<char>(d.begin()+16, d.end())});

                auto temp = max_chunk_id;
                max_chunk_id = chunk_id;

                return temp;

            } else {
                int64_t id = packet_queue.size() - dist - 1;

                if(id < 0 || packet_queue[id].first)
                    return max_chunk_id;

                packet_queue[id].first = true;
                packet_queue[id].second = std::vector<char>(d.begin()+16, d.end());

                return max_chunk_id;
            }
        }

        void clear(){
            chunk_size = 0;
            first_chunk_id = -1;
            buffered_enough = false;
            packet_queue.clear();
        }

        bool isRexmittable(uint64_t chunk_id){
            if(chunk_size == 0)
                return false;

            int64_t cid = chunk_id / chunk_size;
            int64_t mid = max_chunk_id / chunk_size;
            int64_t dist = mid - cid;
            int64_t d_id = packet_queue.size() - 1 - dist;

            if(d_id < 0)
                return false;

            if(d_id >= (int64_t)packet_queue.size()){
                lck(io_mutex);
                std::cerr << "isRexmittable called on a chunk not in the buffer!\n";
                ulck(io_mutex);

                return false;
            }

            return !packet_queue[d_id].first;
        }

        std::vector<char> getPacket(){
            if(packet_queue.empty() || !packet_queue.front().first)
                throw std::runtime_error("NDATAINBUFF");

            auto packet = std::move(packet_queue.front().second);
            packet_queue.pop_front();

            return packet;
        }
};
