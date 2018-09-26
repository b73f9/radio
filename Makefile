all : radio-sender radio-receiver

radio-sender : ./transmitter/*.cpp ./transmitter/*.h
	g++ -std=c++17 -Wall -Wextra -O2 ./transmitter/tr_main.cpp -o ./radio-sender -lboost_program_options -lpthread

radio-receiver: ./receiver/*.cpp ./receiver/*.h
	g++ -std=c++17 -Wall -Wextra -O2 ./receiver/rc_main.cpp ./receiver/rc_radio.cpp -o ./radio-receiver -lboost_program_options -lpthread

clean :
	rm radio-receiver radio-sender ./*.o
