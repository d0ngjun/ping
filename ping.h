#pragma once

#include <chrono>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <string>
#include <vector>

class Ping {
public:
    explicit Ping(const std::string &host);
    ~Ping();

    // 发送ICMP报文
    int send_packet();

    // 收取ICMP报文
    int recv_packet(struct ip *ip, struct icmp *icmp);

    // 打印对象内部状态
    std::string to_string() const;

private:
    // 制作ICMP报文
    struct icmp *make_packet(char *buffer);

    // host的域名或IP
    std::string _host;

    // IPv4专用socket地址,保存目的地址
    struct sockaddr_in _dest;

    // 与host之间的套接字
    int _sock;

    // 报文ID
    unsigned short _id;

    // 报文序列号
    unsigned short _seq;

    std::vector<float> _costs;
};