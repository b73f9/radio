class Timer{
    private:
        Radio *radio;
        std::atomic_flag m = ATOMIC_FLAG_INIT;
        uint64_t rexmit_interval;

        struct rexmit_reminder_t{
                uint64_t session_id;
                std::chrono::system_clock::time_point when_to_rexmit;
                std::vector<uint64_t> chunk_ids;
        };

        std::queue<rexmit_reminder_t> timer_queue;

        void loop(){
            while(true){
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                lck(m);
                while(timer_queue.size() &&
                      timer_queue.front().when_to_rexmit <= std::chrono::system_clock::now()){

                    auto f = timer_queue.front();
                    timer_queue.pop();

                    ulck(m);
                    radio->rexmitReminder(f.chunk_ids, f.session_id);
                    lck(m);
                }
                ulck(m);
            }
        }

    public:
        Timer(Radio *radio, uint64_t rexmit_interval){
            assert(radio != nullptr);

            this->rexmit_interval = rexmit_interval;

            this->radio = radio;
            this->radio->registerTimer(this);

            std::thread rdthread([=]{this->loop();});
            rdthread.detach();
        }

        void setReminder(std::vector<uint64_t> chunk_ids, uint64_t sessid){
            rexmit_reminder_t reminder;
            reminder.chunk_ids = std::move(chunk_ids);
            reminder.session_id = sessid;
            reminder.when_to_rexmit = std::chrono::system_clock::now() +
                    std::chrono::milliseconds(rexmit_interval);

            lck(m);
            timer_queue.push(std::move(reminder));
            ulck(m);
        }
};
