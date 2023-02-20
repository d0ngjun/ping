build: ping.h
	g++ -std=c++14 main.cpp ping.cpp -o ping

clean:
	rm ping
