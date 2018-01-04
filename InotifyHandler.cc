#include "InotifyHandler.hh"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <time.h>
#include <fcntl.h>

#include "common.h"

InotifyHandler::InotifyHandler(Parent* parent)
: Handler(-1), inotify_fd(-1), path(NULL), parent(parent), watched_dir_fd(-1), watched_dir(NULL)
{
}

int InotifyHandler::init(const char* path)
{
    inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd == -1)
        handle_error("inotify_init1");

    watch_descriptor = inotify_add_watch(inotify_fd, path, IN_CREATE | IN_ISDIR);
    if (watch_descriptor == -1)
        handle_error("inotify_add_watch");

    watched_dir = opendir(path);
    if (watched_dir == NULL)
        handle_error("opendir");

    watched_dir_fd = dirfd(watched_dir);
    if (watched_dir_fd < 0)
        handle_error("open watched dir");

    this->path = path;

    return inotify_fd;
}

void InotifyHandler::handle()
{
   /* Some systems cannot read integer variables if they are not
   properly aligned. On other systems, incorrect alignment may
              decrease performance. Hence, the buffer used for reading from
              the inotify file descriptor should have the same alignment as
              struct inotify_event. */

           char buf[8192]
               __attribute__ ((aligned(__alignof__(struct inotify_event))));
           const struct inotify_event *event;
           ssize_t len;
           char *ptr;

           /* Loop while events can be read from inotify file descriptor. */

           for (;;) {

               /* Read some events. */

               len = read(inotify_fd, buf, sizeof buf);
               if (len == -1 && errno != EAGAIN) {
                   handle_error("inotify read");
               }

               /* If the nonblocking read() found no events to read, then
                  it returns -1 with errno set to EAGAIN. In that case,
                  we exit the loop. */

               if (len <= 0)
                   break;

               /* Loop over all events in the buffer */

               for (ptr = buf; ptr < buf + len;
                       ptr += sizeof(struct inotify_event) + event->len) {

                   event = (const struct inotify_event *) ptr;

                   /* Print event type */

                   if (event->mask & IN_OPEN)
                       printf("IN_OPEN: ");
                   if (event->mask & IN_CLOSE_NOWRITE)
                       printf("IN_CLOSE_NOWRITE: ");
                   if (event->mask & IN_CLOSE_WRITE)
                       printf("IN_CLOSE_WRITE: ");

                   /* Print the name of the watched directory */
                   printf("%s/", path);

                   /* Print the name of the file */

                   if (event->len)
                       printf("%s", event->name);


                   /* Print type of filesystem object */

                   if (event->mask & IN_ISDIR)
                       printf(" [directory]\n");
                   else
                       printf(" [file]\n");

                   {
                       pid_t pid = 0;
                       int idx = 0;
                       int n_vals = sscanf(event->name, "child_%d_%d", &pid, &idx);
                       if (n_vals == 2 && pid != 0 && idx != 0) {
                           time_t t = time(NULL);
                           struct tm *tm = localtime(&t);
                           printf("Child idx %d pid %d is dying: %s", idx, pid, asctime(tm));

                           unlinkat(watched_dir_fd, event->name, 0);

		           parent->clear_child(pid);
                           if (!parent->is_exiting()) {
                               parent->respawn(idx);
                           }
                       }
                   }
               }
           }
}

