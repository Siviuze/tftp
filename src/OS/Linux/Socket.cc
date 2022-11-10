#include <netdb.h>
#include <unistd.h>

#include "OS/Socket.h"

namespace tftp
{
    Socket::Socket()
        : fd_{-1}
        , client_size_{sizeof(struct sockaddr_in6)}
    {
        fd_ = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (fd_ < 0)
        {
            throw std::system_error(errno, std::generic_category());
        }
    }


    Socket::Socket(const char* address, int port)
        : Socket()
    {
        target_client_.sin6_family = AF_INET6;
        target_client_.sin6_port = hton(uint16_t(port));
        if (inet_pton(AF_INET6, address, &target_client_.sin6_addr) != 1)
        {
            perror("deducing IPv6 address");
        }
    }

    Socket::~Socket()
    {
        ::close(fd_);
    }

    void Socket::setTimeout(std::chrono::seconds timeout)
    {
        struct timeval posix_timeout;
        posix_timeout.tv_sec  = timeout.count();
        posix_timeout.tv_usec = 0;
        if (setsockopt (fd_, SOL_SOCKET, SO_RCVTIMEO, &posix_timeout, sizeof(struct timeval)) < 0)
        {
            throw std::system_error(errno, std::generic_category());
        }
    }

    int Socket::read(void* data, size_t size)
    {
        return recvfrom(fd_, data, size, 0, (struct sockaddr*)&last_client_, &client_size_);
    }

    int Socket::write(void const* data, size_t size)
    {
        return sendto(fd_, data, size, 0, (struct sockaddr*)&target_client_, client_size_);
    }

    int Socket::bind(char const* address, char const* port)
    {
        struct addrinfo hints;
        struct addrinfo* result;
        struct addrinfo* rp;

        std::memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET6;         // IPv6 only
        hints.ai_socktype = SOCK_DGRAM;     // UDP
        hints.ai_flags = AI_PASSIVE;        // For wildcard IP address
        hints.ai_protocol = IPPROTO_UDP;    // UDP
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;

        int res = getaddrinfo(address, port, &hints, &result);
        if (res != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
            return -1;
        }

        // getaddrinfo() returns a list of address structures. Try to find one that enable bind()
        for (rp = result; rp != nullptr; rp = rp->ai_next)
        {
            if (::bind(fd_, rp->ai_addr, rp->ai_addrlen) == 0)
            {
                break;
            }
        }

        if (rp == nullptr)
        {
            // No solution for bind
            fprintf(stderr, "Could not bind\n");
            return -1;
        }

        freeaddrinfo(result);
        return 0;
    }


    Socket Socket::createSocket()
    {
        Socket s;
        s.target_client_ = last_client_;

        char str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(s.target_client_), str, INET6_ADDRSTRLEN);

        printf("Creating a new socket to communicate with %s : %d\n", str, s.target_client_.sin6_port);
        return s;
    }
}
