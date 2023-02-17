#include "networking.hpp"

namespace SimpicServerLib
{
    simpic_networking_exception::simpic_networking_exception(std::string msg, uint8_t err)
    {
        message = msg;
        errnum = err;
    }

    std::string &simpic_networking_exception::what()
    {
        return message; 
    }

    void recvall(int fd, void *buffer, int length)
    {
        if (recv(fd, buffer, length, MSG_WAITALL) == -1)
        {
            uint8_t err = errno;
            throw simpic_networking_exception("Error recvall(): " + std::string(std::strerror(err)), err);
        }
    }

    void sendall(int fd, void *buffer, int length)
    {
        if (send(fd, buffer, length, 0) == -1)
        {
            uint8_t err = errno;
            throw simpic_networking_exception("Error sendall(): " + std::string(std::strerror(err)), err);
        }
    }

    void new_sendfile(int cfd, int file, size_t file_size)
    {
        off_t offset = 0;
        int total_amnt = 0;
        int amnt = 0;

        while (total_amnt != file_size)
        {
            amnt = sendfile(cfd, file, &offset, 8192);
            total_amnt += amnt;
        }
    }
}