#include <fstream>
#include <filesystem>

#include "tftp/protocol.h"
#include "tftp/OS/Socket.h"

using namespace std::chrono;

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        printf("Usage: client server [put/get] file\n");
        return -1;
    }

    char const* server   = argv[1];
    char const* req      = argv[2];
    char const* filepath = argv[3];

    tftp::Request request;
    request.mode = tftp::Mode::OCTET;
    request.filename = std::filesystem::path(filepath).filename();
    if (strncmp("put", req, 3) == 0)
    {
        request.operation = tftp::opcode::WRQ;
    }
    else if (strncmp("get", req, 3) == 0)
    {
        request.operation = tftp::opcode::RRQ;
    }
    else
    {
        printf("Only put or get action are supported\n");
        return -1;
    }

    tftp::Socket socket(server, 69);
    socket.setTimeout(5s);

    // set options
    request.window_size.value = 32;
    request.window_size.is_enable = true;
    request.block_size.value  = 1024;
    request.block_size.is_enable = true;

    std::vector<char> packet = forgeRequest(request);
    if (socket.write(packet.data(), packet.size()) < 0)
    {
        perror("write request");
        return -1;
    }

    packet.resize(512);
    int rec = socket.read(packet.data(), packet.size());
    if (rec < 0)
    {
        perror("read request reply");
        return -1;
    }
    socket.switchToLast();

    if (tftp::getOpcode(packet.data(), packet.size()) == tftp::opcode::ERROR)
    {
        int code;
        std::string msg;
        tftp::parseError(packet.data(), packet.size(), code, msg);

        printf("Error recevied from server: <%s>\n", msg.c_str());
        return 2;
    }

    // check OACK answer (if server can handles our options)
    if (tftp::parseOptionAck(packet.data(), rec, request) == 0)
    {
        if (request.operation == tftp::opcode::RRQ)
        {
            packet = tftp::forgeAck(0);
            if (socket.write(packet.data(), packet.size()) < 0)
            {
                perror("write request");
                return -1;
            }
        }
    }
    else
    {
        int block = tftp::parseAck(packet.data(), rec);
        if (block < 0)
        {
            perror("Parse ack");
            return -1;
        }
        if (block != 0)
        {
            printf("Ack value is unexpected: %d\n", block);
            return -1;
        }
    }
    printf("opcode      : %x\n",   request.operation);
    printf("mode        : %s\n",   toString(request.mode));
    printf("filename    : %s\n", request.filename.c_str());
    for (auto const& option : request.supported_options)
    {
        printf("%-12s: %-4ld (%d)\n", option->name, option->value, option->is_enable);
    }

    std::fstream file;
    if (request.operation == tftp::opcode::WRQ)
    {
        file.open(filepath, std::fstream::in | std::fstream::binary);
        tftp::processRead(request, socket, file);
    }
    else
    {
        file.open(filepath, std::fstream::out | std::fstream::binary | std::fstream::trunc);
        tftp::processWrite(request, socket, file);
    }

    return 0;
}
