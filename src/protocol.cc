#include "protocol.h"

#include <functional>

namespace tftp
{
    char const* errnoToString(int code)
    {
        switch (code)
        {
            case ENOENT: { return "File not found.";                    }
            case EACCES: { return "Access violation.";                  }
            case ENOMEM: { return "Disk full or allocation exceeded.";  }
            case EPROTO: { return "Illegal TFTP operation.";            }
            case EPERM:  { return "Unknown transfer ID.";               }
            case EEXIST: { return "File already exists.";               }
            case EUSERS: { return "No such user.";                      }
            default:
            {
                return strerror(code);
            }
        }
    }


    template<> void insert<char const*>(std::vector<char>& buffer, char const*const& str)
    {
        char const* data_begin = str;
        char const* data_end   = str + strlen(str);
        buffer.insert(buffer.end(), data_begin, data_end);

        char const str_end = 0;
        buffer.insert(buffer.end(), str_end);
    }


    template<> void insert<std::string>(std::vector<char>& buffer, std::string const& str)
    {
        char const* data_begin = str.data();
        char const* data_end   = str.data() + str.size();
        buffer.insert(buffer.end(), data_begin, data_end);

        char const str_end = 0;
        buffer.insert(buffer.end(), str_end);
    }


    size_t maxSize(char const* data, size_t size, char const* current_pos)
    {
        return size - (current_pos - data);
    }


    size_t entryLen(char const* data, size_t size, char const* current_pos)
    {
        return strnlen(current_pos, maxSize(data, size, current_pos)) + 1;
    }


    bool extractOption(char const* data, size_t size, Request& req, char const*& position)
    {
        for (auto option : req.supported_options)
        {
            if (strncasecmp(option->name, position, maxSize(data, size, position)) == 0)
            {
                position += entryLen(data, size, position);

                size_t len = entryLen(data, size, position);
                auto result = std::from_chars(position, position + len, option->value);
                position += len;

                if (result.ec != std::errc())
                {
                    return false;
                }

                option->value = std::clamp(option->value, option->min, option->max);
                option->is_enable = true;
                return true;
            }
        }
        return false;
    }


    int parseRequest(char const* data, size_t size, Request& request)
    {
        if ((size < 8) or (size > 512)) // min request size is 8 -> opcode (2) + filename 0 (1) + mode 'mail' (4) + mode 0 (1)
        {
            request.operation = opcode::ILLEGAL;
            errno = EMSGSIZE;
            return -1;
        }
        char const* pos = data;

        request.operation = hton(*reinterpret_cast<uint16_t const*>(pos));
        if ((request.operation != opcode::RRQ) and (request.operation != opcode::WRQ))
        {
            // Cannot continue parsing this network packet
            errno = EPROTO;
            return -1;
        }
        pos += 2;

        // Extract file name (always first)
        std::size_t len = entryLen(data, size, pos);
        request.filename = std::string(pos, len);
        pos += len;

        // Extract mode
        char const* mode_str = pos;
        size_t max_size = maxSize(data, size, pos);
        for (Mode mode = Mode::NETASCII; mode < Mode::INVALID; mode = Mode(mode + 1))
        {
            if (strncasecmp(toString(mode), mode_str, max_size) == 0)
            {
                request.mode = mode;
                pos += strlen(toString(mode) + 1);
                break;
            }
        }

        // Parse options
        while ((pos - data) < static_cast<ssize_t>(size))
        {
            if (extractOption(data, size, request, pos) == true)
            {
                continue;
            }

            pos += entryLen(data, size, pos); // unknown option -> skip
        }

        return 0;
    }


    std::vector<char> forgeRequest(Request const& request)
    {
        std::vector<char> buffer;
        buffer.reserve(512);    // max size of request packet

        // write opcode
        uint16_t const opcode = hton(request.operation);
        insert(buffer, opcode);

        // write filename
        insert(buffer, request.filename);

        // write mode
        insert(buffer, toString(request.mode));

        // write options
        for (auto const& option : request.supported_options)
        {
            if (not option->is_enable)
            {
                continue;
            }

            insert(buffer, option->name);
            insert(buffer, std::to_string(option->value));
        }

        return buffer;
    }


    int parseOptionAck(char const* data, size_t size, Request& request)
    {
        if ((size < 4) or (size > 512)) // min request size is 4 -> opcode (2) + optname 0 (1) + optvalue 0 (1)
        {
            errno = EMSGSIZE;
            return -1;
        }
        char const* pos = data;

        opcode operation = hton(*reinterpret_cast<opcode const*>(pos));
        if (operation != opcode::OACK)
        {
            // Cannot continue parsing this network packet
            errno = EPROTO;
            return -1;
        }
        pos += 2;

        // Parse options
        while ((pos - data) < static_cast<ssize_t>(size))
        {
            if (extractOption(data, size, request, pos) == true)
            {
                continue;
            }

            // Unknown option: it shall never happens since server shall only respond with client requested options
            errno = EPROTO;
            return -1;
        }

        return 0;
    }


    std::vector<char> forgeOptionAck(Request const& request)
    {
        std::vector<char> buffer;
        buffer.reserve(512);    // max size of packet: oack cannot be bigger

        // write opcode
        uint16_t const opcode = hton(opcode::OACK);
        insert(buffer, opcode);

        for (auto const& option : request.supported_options)
        {
            if (not option->is_enable)
            {
                continue;
            }

            insert(buffer, option->name);
            insert(buffer, std::to_string(option->value).c_str());
        }

        if (buffer.size() == 2)
        {
            // No options supported
            buffer.clear();
        }

        return buffer;
    }


    bool isLastDataPacket(size_t size, Request const& request)
    {
        return ((size - 4) < static_cast<size_t>(request.block_size.value));
    }


    int parseData(char const* data, size_t size)
    {
        char const* pos = data;

        if (size < 4)
        {
            errno = EMSGSIZE;
            return -1;
        }

        uint16_t const operation = hton(*reinterpret_cast<uint16_t const*>(pos));
        if (operation != opcode::DATA)
        {
            errno = EPROTO;
            return -1;
        }
        pos += 2;

        uint16_t const block_id = hton(*reinterpret_cast<uint16_t const*>(pos));
        pos += 2;

        return static_cast<int>(block_id);
    }


    std::vector<char> forgeData(Request const& request, int block, std::istream& input)
    {
        std::vector<char> buffer;
        int max_size = request.block_size.value + 4;
        buffer.reserve(max_size);

        // write opcode
        uint16_t const opcode = hton(opcode::DATA);
        insert(buffer, opcode);

        // write block id
        uint16_t const block_id = hton(static_cast<uint16_t>(block));
        insert(buffer, block_id);

        // write data
        buffer.resize(max_size);
        input.read(buffer.data() + 4, request.block_size.value);
        if (not input)
        {
            // cut unread part
            buffer.resize(input.gcount() + 4);
        }

        return buffer;
    }


    int parseAck(char const* data, size_t size)
    {
        char const* pos = data;

        if (size != 4)
        {
            errno = EMSGSIZE;
            return -1;
        }

        uint16_t const operation = hton(*reinterpret_cast<uint16_t const*>(pos));
        if (operation != opcode::ACK)
        {
            errno = EPROTO;
            return -1;
        }
        pos += 2;

        uint16_t const block_id = hton(*reinterpret_cast<uint16_t const*>(pos));
        return static_cast<int>(block_id);
    }


    std::vector<char> forgeAck(int block_number)
    {
        std::vector<char> buffer;
        buffer.reserve(4);

        // write opcode
        uint16_t const opcode = hton(opcode::ACK);
        insert(buffer, opcode);

        // write block number
        uint16_t const block = hton(static_cast<uint16_t>(block_number));
        insert(buffer, block);

        return buffer;
    }


    std::vector<char> forgeError(int code)
    {
        std::vector<char> buffer;
        buffer.reserve(512);

        // write opcode
        uint16_t const opcode = hton(opcode::ERROR);
        insert(buffer, opcode);

        // write error code
        uint16_t const errorCode = hton(errnoToTftp(code));
        insert(buffer, errorCode);

        // write message
        char const* message = errnoToString(code);
        insert(buffer, message);

        // done
        return buffer;
    }

    //TODO:
    // 1. use an absolute block id on top of rolling block id (uint16_t in the protocol)
    // 2. use absolute block id to set the file cursor at the right place to handle errors
    void processRead(Request const& request, AbstractSocket& socket, std::istream& file)
    {
        bool isTransferFinish = false;
        auto writeData = [&](int block)
        {
            for (uint32_t i = 0; i < request.window_size.value; ++i)
            {
                auto dataPacket = forgeData(request, block, file);
                int written = socket.write(dataPacket);
                if (written < 0)
                {
                    return -1;
                }

                if (tftp::isLastDataPacket(dataPacket.size(), request))
                {
                    isTransferFinish = true;
                    break;
                }

                ++block;
            }
            return 0;
        };

        try
        {
            int retry = 0;
            int window_block = 1;
            int absolute_block = 1;
            while (not isTransferFinish)
            {
                if (retry > MAX_RETRY)
                {
                    throw std::system_error(EIO, std::generic_category());
                }

                // Set file read cursor on absolute block position (so that acked block is always synced with file cursor position)
                // -1 because the first index is 1, not 0
                file.seekg((absolute_block - 1) * request.block_size.value);

                int ret = writeData(window_block);
                if (ret < 0)
                {
                    ++retry;
                    continue;
                }

                std::vector<char> ack;
                ack.resize(4);
                if (socket.read(ack) < 0)
                {
                    ++retry;
                    isTransferFinish = false;
                    continue;
                }

                int ack_block = parseAck(ack.data(), ack.size());
                if (ack_block < 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }
                uint16_t sent_blocks = ack_block + 1 - window_block;
                absolute_block += sent_blocks;
                window_block = ack_block + 1; // next block to send
            }
            printf("sent %d blocks\n", absolute_block);
        }
        catch(std::system_error& e)
        {
            auto reply = tftp::forgeError(e.code().value());
            socket.write(reply);
            printf("error: %s\n", e.what());
        }
    }


    //TODO:
    // 1. use an absolute block id on top of rolling block id (uint16_t in the protocol)
    // 2. use absolute block id to set the file cursor at the right place to handle errors
    void processWrite(Request const& request, AbstractSocket& socket, std::ostream& file)
    {
        std::vector<char> packet;
        packet.resize(request.block_size.value + 4);

        bool isTransferFinish = false;
        auto readData = [&](int last_acked_block)
        {
            uint16_t expected_block = last_acked_block + 1;
            int last_written_block = last_acked_block;
            int block = 0;
            for (int32_t i = 0; i < request.window_size.value; ++i)
            {
                int rec = socket.read(packet);
                if (rec < 0)
                {
                    return -1;
                }

                block = tftp::parseData(packet.data(), rec);
                if (block < 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }

                // Check that the block id is the one expected, if not skip it
                if (expected_block != block)
                {
                    printf("DROP %d - expected: %d\n", block, expected_block);
                    continue;
                }

                // TODO handle netascii
                file.write(packet.data() + 4, rec - 4);
                expected_block = block + 1;
                last_written_block = block;

                if (tftp::isLastDataPacket(rec, request))
                {
                    isTransferFinish = true;
                    break;
                }
            }

            return last_written_block;
        };

        try
        {
            int retry = 0;
            int last_acked_block = 0;
            while (not isTransferFinish)
            {
                if (retry > MAX_RETRY)
                {
                    throw std::system_error(EIO, std::generic_category());
                }

                int block = readData(last_acked_block);
                if (block < 0)
                {
                    ++retry;
                    continue;
                }

                std::vector<char> reply = tftp::forgeAck(block);
                if (socket.write(reply) < 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }

                last_acked_block = block;
                retry = 0;  // reset retry after every success
            }
        }
        catch(std::system_error& e)
        {
            auto reply = tftp::forgeError(e.code().value());
            socket.write(reply);
            printf("error: %s\n", e.what());
        }
    }
}
