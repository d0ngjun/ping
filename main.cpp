#include "ping.h"

#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>

// 发送报文数
constexpr int SEND_NUM = 10;

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cout << "usage: " << argv[0] << " [hostname/IP address]\n";
		return 1;
	}

    try {
        Ping ping(argv[1]);
        struct ip ip;
        struct icmp icmp;

        std::cout << "ping " << argv[1] << "\n";

        for (int i = 0; i < SEND_NUM; ++i) {
            int rv = ping.send_packet();
            if (rv < 0) {
                std::cout << "Ping::send_packet failed: " << rv << "\n";
                continue;
            }

            rv = ping.recv_packet(&ip, &icmp);
            if (rv < 0) {
                std::cout << "Ping::recv_packet failed: " << rv << "\n";
                continue;
            }

            std::cout << ip.ip_len << " bytes from " << inet_ntoa(ip.ip_src) << ": icmp seq = " << icmp.icmp_seq << ", ttl = " << (int)ip.ip_ttl << ", time = " << rv << " ms\n";

            sleep(1);
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

	return 0;
}