#include <sys/stat.h>

#include <fstream>
#include <cstring>

#include "tftp/protocol.h"
#include "tftp/OS/Socket.h"


int main()
{
    // Create UDP socket
    tftp::Socket listener;
    if (listener.bind("::", "69"))
    {
        return -1;
    }
    printf("Socket created successfully\n");
    printf("Listening for incoming messages...\n\n");

    // Receive client's message:
    while (true)
    {
        char request_buffer[512];
        int rec = listener.read(request_buffer, sizeof(request_buffer));
        if (rec < 0)
        {
            perror("Couldn't receive\n");
            return -1;
        }

        // Parse
        tftp::Request request;
        int ret = tftp::parseRequest(request_buffer, rec, request);
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
            printf("%-12s: %-4ld (%d)\n", option->name, option->value, option->is_enable);
        }

        auto begin = std::chrono::steady_clock::now();

        // Start transfer
        tftp::Socket transferSocket = listener.createSocket();
        std::fstream file;

        std::vector<char> reply = tftp::forgeOptionAck(request);
        if (request.operation == tftp::opcode::WRQ)
        {
            file.open(request.filename, std::fstream::out | std::fstream::binary | std::fstream::trunc);
            if (reply.size() == 0)
            {
                reply = tftp::forgeAck(0);
            }
            transferSocket.write(reply);
            tftp::processWrite(request, transferSocket, file);
        }
        else
        {
            file.open(request.filename, std::fstream::in | std::fstream::binary);
            if (reply.size() != 0)
            {
                // send OACK
                transferSocket.write(reply);

                // wait for OACK ack (0)
                char ack[4];
                transferSocket.read(ack, 4);
                int oack_ack = tftp::parseAck(ack, 4);
                if (oack_ack != 0)
                {
                    printf("Oops\n");
                    continue; // Abort transfer
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

    return 0;
}
