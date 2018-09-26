class StdOutWriter{
    private:
        Radio *radio;
        std::vector<char> data;
        std::atomic_flag startWriting = ATOMIC_FLAG_INIT;

    public:
        StdOutWriter(Radio *radio){
            std::ios_base::sync_with_stdio(false);
            this->radio = radio;
            lck(startWriting);
            radio->registerOutputter(this);
            radio->stdOutReady();
        }

        void loop(){
            while(true){
                lck(startWriting);
                write(1, data.data(), data.size());
                radio->stdOutReady();
            }
        }

        void outputData(std::vector<char> &&Data){
            data = std::move(Data);
            ulck(startWriting);
        }
};
