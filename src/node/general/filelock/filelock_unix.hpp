#pragma once
#include <stdexcept>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

class Filelock {
  public:
    Filelock(const std::string &path) {
#ifndef __APPLE__
        if (path == "") 
            return ;
        fd = open(path.c_str(), 0);
        if (fd < 0) {
            throw std::runtime_error("Cannot open file \"" + path +
                                     "\": " + strerror(errno));
        }
        if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
            throw std::runtime_error("Another instance is accessing the database \"" + path +
                    "\": " + strerror(errno));
        }
#endif
    };
    Filelock(const Filelock &) = delete;
    ~Filelock() {
        if (fd > 0) {
            close(fd);
        }
    };

  private:
    int fd = -1;
};
