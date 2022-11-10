#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include <fstream>

#include "tftp/protocol.h"

class LinuxSocket final : public tftp::AbstractSocket
{
public:
    LinuxSocket(struct sockaddr_in6 client)
        : fd_{-1}
        , client_{client}
        , client_size_{sizeof(struct sockaddr_in6)}
    {
        fd_ = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (fd_ < 0)
        {
            throw std::system_error(errno, std::generic_category());
        }

    }

    void setTimeout(std::chrono::seconds timeout) override
    {
        struct timeval posix_timeout;
        posix_timeout.tv_sec  = timeout.count();
        posix_timeout.tv_usec = 0;
        if (setsockopt (fd_, SOL_SOCKET, SO_RCVTIMEO, &posix_timeout, sizeof(struct timeval)) < 0)
        {
            throw std::system_error(errno, std::generic_category());
        }
    }

    int read(void* data, size_t size) override
    {
        struct sockaddr_in6 client;
        return recvfrom(fd_, data, size, 0, (struct sockaddr*)&client, &client_size_);
    }

    int write(void const* data, size_t size) override
    {
        return sendto(fd_, data, size, 0, (struct sockaddr*)&client_, client_size_);
    }

private:
    int fd_;
    struct sockaddr_in6 client_;
    socklen_t client_size_;
};


int main()
{
    struct sockaddr_in6 server_addr, client_addr;
    char client_message[2000];
    socklen_t client_struct_length = sizeof(client_addr);

    // Create UDP socket:
    int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        perror("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(69);
    server_addr.sin6_addr = in6addr_any;

    // Bind to the set port and IP:
    if (bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Couldn't bind to the port\n");
        return -1;
    }
    printf("Listening for incoming messages...\n\n");

    // Receive client's message:
    while (true)
    {
        ssize_t rec = recvfrom(fd, client_message, sizeof(client_message), 0, (struct sockaddr*)&client_addr, &client_struct_length);
        if (rec < 0)
        {
            perror("Couldn't receive\n");
            return -1;
        }
        printf("Received %ld bytes from IP: TODO and port: %i\n", rec, ntohs(client_addr.sin6_port));

        // Parse
        tftp::Request request;
        int ret = tftp::parseRequest(client_message, rec, request);
        if (ret != 0)
        {
            printf("aie: %s\n", strerror(ret));
            return -1;
        }
        printf("opcode      : %x\n",   request.operation);
        printf("mode        : %s\n",   toString(request.mode));
        printf("filename    : %s\n", request.filename.c_str());
        for (auto const& option : request.supported_options)
        {
            printf("%-12s: %-4d (%d)\n", option->name, option->value, option->is_enable);
        }

        auto begin = std::chrono::steady_clock::now();

        // Start transfer
        LinuxSocket transferSocket(client_addr);
        std::fstream file;

        std::vector<char> reply = tftp::forgeOptionAck(request);
        if (request.operation == tftp::opcode::WRQ)
        {
            file.open(request.filename, std::fstream::out | std::fstream::binary | std::fstream::trunc);
            if (reply.size() == 0)
            {
                reply = tftp::forgeAck(0);
            }
            transferSocket.write(reply.data(), reply.size());
            tftp::processWrite(request, transferSocket, file);
        }
        else
        {
            file.open(request.filename, std::fstream::in | std::fstream::binary);
            if (reply.size() != 0)
            {
                // send OACK
                transferSocket.write(reply.data(), reply.size());

                // wait for OACK ack (0)
                char ack[4];
                transferSocket.read(ack, 4);
                int oack_ack = tftp::parseAck(ack, 4);
                if (oack_ack != 0)
                {
                    printf("Oops\n");
                }
            }
            tftp::processRead(request, transferSocket, file);
        }

        struct stat stat_buf;
        (void) stat(request.filename.c_str(), &stat_buf);
        double file_size = stat_buf.st_size / 1024.0 / 1024.0;
        file.close();

        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0;
        std::cout << "Transfer " << file_size << "MB in " << elapsed << "s" << std::endl;
        std::cout << "-> " << file_size / elapsed << "MB/s" << std::endl;
    }

    // Close the socket:
    close(fd);

    return 0;
}
