#pragma once

#include "Handler.hh"
#include "Parent.hh"

#include <dirent.h>

class InotifyHandler: public Handler
{
public:
    InotifyHandler(Parent* parent);

    int init(const char* path);

    virtual void handle();

protected:
    int inotify_fd;
    int watch_descriptor;
    const char* path;
    Parent* parent;
    int watched_dir_fd;
    DIR* watched_dir;
};

