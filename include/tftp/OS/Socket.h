#ifndef TFTP_OS_LINUX_SOCKET_H
#define TFTP_OS_LINUX_SOCKET_H

#include <sys/socket.h>
#include <arpa/inet.h>

#include "tftp/protocol.h"

namespace tftp
{
    class Socket final : public tftp::AbstractSocket
    {
    public:
        Socket();
        Socket(const char* address, int port);
        virtual ~Socket();

        void setTimeout(std::chrono::seconds timeout) override;
        int read(void* data, size_t size) override;
        int write(void const* data, size_t size) override;

        int bind(char const* address, char const* port);
        Socket createSocket();
        void switchToLast();


    private:
        int fd_;
        struct sockaddr_in6 target_client_{};
        struct sockaddr_in6 last_client_{};
        socklen_t client_size_;
    };
}

#endif
