#include <fstream>

#include "tftp/protocol.h"
#include "tftp/OS/Socket.h"

using namespace std::chrono;

int main()
{
    tftp::Socket socket("::1", 69);
    socket.setTimeout(5s);

    tftp::Request request;
    request.filename = "yolo_file";
    request.operation = tftp::opcode::WRQ;
    request.mode = tftp::Mode::OCTET;

    std::vector<char> packet = forgeRequest(request);
    if (socket.write(packet.data(), packet.size()) < 0)
    {
        perror("write request");
        return -1;
    }

    int rec = socket.read(packet.data(), packet.size());
    if (rec < 0)
    {
        perror("read ack request");
        return -1;
    }
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

    std::fstream file;
    if (request.operation == tftp::opcode::WRQ)
    {
        file.open(request.filename, std::fstream::in | std::fstream::binary);
        tftp::processRead(request, socket, file);
    }
    else
    {
        file.open(request.filename, std::fstream::out | std::fstream::binary | std::fstream::trunc);
        tftp::processWrite(request, socket, file);
    }

    return 0;
}