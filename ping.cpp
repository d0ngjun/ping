#include "error.h"
#include "ping.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

constexpr size_t PACKET_LEN = offsetof(struct icmp, icmp_dun) + sizeof(std::chrono::duration<double>);

static u_int16_t compute_cksum(struct icmp *icmp, size_t len)
{
    u_int16_t *data = (u_int16_t *)icmp;
    u_int32_t sum = 0;

    while (len > 1)
    {
        sum += *data++;
        len -= 2;
    }

    if (len == 1)
        sum += *data & 0xff00;

    // ICMP校验和带进位
    return ~((sum >> 16) + (sum & 0xffff));
}

Ping::Ping(const std::string &host): _host(host)
{
    // 创建ICMP套接字
    // AF_INET: IPv4, SOCK_RAW: IP协议数据报接口, IPPROTO_ICMP: ICMP协议
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0)
        throw PingError(host, "socket", errno);

    // 设置超时时间：1秒
	struct timeval tv {1, 0};

	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        int e = errno;
        close(sock);
		throw PingError(host, "setsockopt", e);
	}

	_dest.sin_family = AF_INET;

    // 将点分十进制ip地址转换为网络字节序
	in_addr_t addr = inet_addr(host.c_str());
	if (addr == INADDR_NONE) {
		// 转换失败，表明是主机名,需通过主机名获取ip
		struct hostent *hostent = gethostbyname(host.c_str());
		if (hostent == nullptr) {
			close(sock);
			throw PingError(host, "gethostbyname", h_errno);
		}

		memcpy(&_dest.sin_addr, hostent->h_addr_list[0], hostent->h_length);
	} else {
		memcpy(&_dest.sin_addr, &addr, sizeof(struct in_addr));
	}

	_sock = sock;
    _id = getpid() & 0xffff;
    _seq = 0;
}

struct SquareSum
{
	void operator()(float n)
	{
		sum += n * n;
	}

	int sum{0};
};

Ping::~Ping()
{
    close(_sock);

    if (_costs.size() == 0)
        return;

    float loss = double(_seq - _costs.size()) / _seq;

    float min = *std::min_element(_costs.begin(), _costs.end());
    float max = *std::max_element(_costs.begin(), _costs.end());
    float avg = std::accumulate(_costs.begin(), _costs.end(), 0.0f) / _costs.size();

    auto sq = std::for_each(_costs.begin(), _costs.end(), SquareSum());
    float stddev = std::sqrt(sq.sum / _costs.size() - avg * avg);

    std::cout << "--- " << _host << " ping statistics ---\n"
            << _seq << " packets transmitted, " << _costs.size() << " received, " << loss * 100 << "% packet loss\n"
            << "rtt min/avg/max/stddev = " << min << "/" << max << "/" << avg << "/" << stddev << " ms\n";
}

struct icmp *Ping::make_packet(char *buffer)
{
    auto *icmp = (struct icmp *)buffer;

    // 类型和代码分别为ICMP_ECHO，0代表请求回送
    icmp->icmp_type = ICMP_ECHO;

    icmp->icmp_code = 0;

    // 校验和需要先设置为0，防止后面计算的校验和不正确
    icmp->icmp_cksum = 0;

    // 进程号作为ID
    icmp->icmp_id = _id;

    // 序号
    icmp->icmp_seq = ++_seq;

    // 发送的数据为当前系统时间
    auto send_time = std::chrono::system_clock::now();
    memcpy(icmp->icmp_data, &send_time, sizeof(send_time));

    // 校验和
    icmp->icmp_cksum = compute_cksum(icmp, PACKET_LEN);

    return icmp;
}

int Ping::send_packet()
{
    // ICMP header + chrono::duration.
    char buffer[offsetof(struct icmp, icmp_dun) + sizeof(std::chrono::duration<double>)];

    return sendto(_sock, make_packet(buffer), sizeof(buffer), 0, (struct sockaddr *)&_dest, sizeof(struct sockaddr_in));
}

int Ping::recv_packet(struct ip *ip, struct icmp *icmp)
{
    // IP header + ICMP header + chrono::duration.
    char buffer[sizeof(struct ip) + offsetof(struct icmp, icmp_dun) + sizeof(std::chrono::duration<double>)];

    int addrlen = sizeof(struct sockaddr_in);

    int recv_bytes = recvfrom(_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&_dest, (socklen_t *)&addrlen);
    if (recv_bytes < 0)
        return -errno;

    auto *ip_header = (struct ip *)buffer;
    auto *icmp_header = (struct icmp *)(buffer + ip_header->ip_hl * 4);

    // 判断接收到的报文是否是自己所发报文的响应
    if ((icmp_header->icmp_type != ICMP_ECHOREPLY) || (icmp_header->icmp_id != _id))
        return -EINVAL;

    // 仅拷贝header，不需要数据
    memcpy(ip, ip_header, ip_header->ip_hl * 4);
    memcpy(icmp, icmp_header, offsetof(struct icmp, icmp_dun));

    auto *send_time = (std::chrono::time_point<std::chrono::system_clock> *)icmp_header->icmp_data;
    auto recv_time = std::chrono::system_clock::now();

	// 转化为毫秒
    float rtt = (recv_time - *send_time).count() / 1000;

    _costs.emplace_back(rtt);

    return (int)rtt;
}
