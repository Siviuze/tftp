#ifndef TFTP_PROTOCOL_H
#define TFTP_PROTOCOL_H

#include <chrono>
#include <vector>
#include <array>
#include <string>
#include <iostream>

namespace tftp
{
    constexpr int MAX_RETRY = 5;

    enum opcode : uint16_t
    {
        RRQ     = 1,  // Read request
        WRQ     = 2,  // Write request
        DATA    = 3,  // Data
        ACK     = 4,  // Acknowledgment
        ERROR   = 5,  // Error
        OACK    = 6,  // Option Acknowledgment
        ILLEGAL
    };

    enum error_code : uint16_t
    {
        CUSTOM              = 0,
        FILE_NOT_FOUND      = 1,
        ACCESS_VIOLATION    = 2,
        NO_MEMORY           = 3,
        ILLEGAL_OPERATION   = 4,
        UNKNOWN_ID          = 5,
        FILE_EXIST          = 6,
        UNKNOWN_USER        = 7,
        NEGOTIATION_FAILURE = 8,

        // Custom codes (to be sent through CUSTOM code)
        CUSTOM_CODE_SECTION = 0x100,
        RETRY_EXCEEDED      = 0x101,
        IO                  = 0x102,
        SOCKET_UNUSABLE     = 0x103
    };
    constexpr char const* toString(enum error_code code)
    {
        switch (code)
        {
            case FILE_NOT_FOUND:        { return "File not found";                   }
            case ACCESS_VIOLATION:      { return "Access violation";                 }
            case NO_MEMORY:             { return "Disk full or allocaiton exceeded"; }
            case ILLEGAL_OPERATION:     { return "Illegal TFTP operation";           }
            case UNKNOWN_ID:            { return "Unknown transfer ID";              }
            case FILE_EXIST:            { return "File already exists";              }
            case NEGOTIATION_FAILURE:   { return "Option negotiation failure";       }
            case RETRY_EXCEEDED:        { return "Retry exceeded";                   }
            case IO:                    { return "I/O error";                        }
            case SOCKET_UNUSABLE:       { return "Socket unusable";                  }
            default:                    { return "unknown";                          }
        }
    }

    enum Mode
    {
        NETASCII,
        OCTET,
        MAIL,
        INVALID
    };
    constexpr char const* toString(enum Mode mode)
    {
        switch (mode)
        {
            case (NETASCII): { return "netascii"; }
            case (OCTET):    { return "octet";    }
            case (MAIL):     { return "mail";     }
            default:         { return "unknown";  }
        }
    }

    struct Option
    {
        char const* name;
        int64_t value;
        int64_t const default_value;
        int64_t min;
        int64_t max;
        bool is_enable{false};
    };
    constexpr Option BLKSIZE    = {"blksize",     512, 512, 8, 65464      };
    constexpr Option WINDOWSIZE = {"windowsize",  1,     1, 1, 65535      };
    constexpr Option TIMEOUT    = {"timeout",     1,     1, 5, 255        };
    constexpr Option TSIZE      = {"tsize",       0,     0, 0, INT64_MAX  };

    struct Request
    {
        uint16_t operation{ILLEGAL};
        std::string filename;
        enum Mode mode{INVALID};

        Option block_size   {BLKSIZE};
        Option window_size  {WINDOWSIZE};
        Option timeout      {TIMEOUT};
        Option transfer_size{TSIZE};
        std::array<Option*, 2> supported_options = { &block_size, &window_size };
    };

    class AbstractSocket
    {
    public:
        int read(std::vector<char>& packet)
        {
            return read(packet.data(), packet.size());
        }

        int write(std::vector<char> const& packet)
        {
            return write(packet.data(), packet.size());
        }

        virtual void setTimeout(std::chrono::seconds timeout) = 0;
        virtual int read(void* data, size_t size) = 0;
        virtual int write(void const* data, size_t size) = 0;
    };


    // data management helpers
    template<typename T>
    T hton(T value)
    {
        uint8_t* buffer = reinterpret_cast<uint8_t*>(&value);
        if constexpr(sizeof(T) == 2)
        {
            return (T)(buffer[0] << 8 | buffer[1]);
        }
        else if constexpr(sizeof(T) == 4)
        {
            return (T)(buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3]);
        }
        else
        {
            std::abort();
        }
    }

    template<typename T>
    void insert(std::vector<char>& buffer, T const& data)
    {
        uint8_t const* data_begin = reinterpret_cast<uint8_t const*>(&data);
        uint8_t const* data_end   = data_begin + sizeof(T);
        buffer.insert(buffer.end(), data_begin, data_end);
    }
    template<> void insert<char const*>(std::vector<char>& buffer, char const* const& str);
    template<> void insert<std::string>(std::vector<char>& buffer, std::string const& str);

    size_t maxSize(char const* data, size_t size, char const* current_pos);
    size_t entryLen(char const* data, size_t size, char const* current_pos);
    bool extractOption(char const* data, size_t size, Request& req, char const*& position);

    // Protocol management
    opcode getOpcode(char const* data, size_t size);

    int parseRequest(char const* data, size_t size, Request& request);
    std::vector<char> forgeRequest(Request const& request);

    int parseOptionAck(char const* data, size_t size, Request& request);
    std::vector<char> forgeOptionAck(Request const& request);

    bool isLastDataPacket(size_t size, Request const& request); //< size of the whole packet
    int parseData(char const* data, size_t size);
    std::vector<char> forgeData(Request const& request, int block, std::istream& input);

    int parseAck(char const* data, size_t size);
    std::vector<char> forgeAck(int block_number);

    int parseError(char const* data, size_t size, enum error_code& code, std::string& error_string);
    std::vector<char> forgeError(enum error_code code);

    // read and writes functions that can be used for both server and client
    void processRead(Request const& request, AbstractSocket& socket, std::istream& file);
    void processWrite(Request const& request, AbstractSocket& socket, std::ostream& file);
}

#endif
