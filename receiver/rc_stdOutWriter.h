class StdOutWriter{
    private:
        Radio *radio;

    public:
        StdOutWriter(Radio *radio){
            std::ios_base::sync_with_stdio(false);
            this->radio = radio;
            radio->registerOutputter(this);
        }

        void loop(){
            while(true){
                std::vector<char> data = radio->getData();
                write(1, data.data(), data.size());
            }
        }
};
