/*
 * Apocatarsis 2016
 * Released with absolutely no warranty, use with your own responsibility.
 *
 * This version is still in development
 */

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <system_error>

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/select.h>

#define DEV_NUM_KEYS 12
#define EXTRA_BUTTONS 2
#define OFFSET 263

using namespace std;

static constexpr auto devices = {
    // NAGA EPIC
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga_Epic-if01-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga_Epic-event-mouse"),
    // NAGA EPIC DOCK
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-if01-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-event-mouse"),
    // NAGA 2014
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga_2014-if02-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga_2014-event-mouse"),
    // NAGA MOLTEN
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga-if01-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga-event-mouse"),
    // NAGA EPIC CHROMA
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-if01-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-event-mouse"),
    // NAGA EPIC CHROMA DOCK
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-if01-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-event-mouse"),
    // NAGA CHROMA
    make_pair("/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-if02-event-kbd",
              "/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-event-mouse")
};

struct RaiiFd
{
    explicit RaiiFd(const char *path, int oflag)
        : fd(open(path, oflag))
    {
        if (fd == -1)
            throw system_error(errno, system_category());
    }

    RaiiFd(RaiiFd &&o)
        : fd(o.fd)
    {
        o.fd = -1;
    }

    ~RaiiFd()
    {
        if (fd != -1)
            close(fd);
    }

    RaiiFd& operator=(RaiiFd &&o)
    {
        if (fd != -1)
            close(fd);

        fd = o.fd;
        o.fd = -1;
    }

    RaiiFd(const RaiiFd&) = delete;
    RaiiFd& operator=(const RaiiFd&) = delete;

    int fd;
};

class NagaDaemon {
    struct input_event ev1[64], ev2[64];
    int id, size;
    // because we don't have optional<T>
    unique_ptr<RaiiFd> side_btn, extra_btn;

    vector<string> args {
        "enlightenment_remote -desktop-show 0 0",
        "enlightenment_remote -desktop-show 1 0",
        "enlightenment_remote -desktop-show 2 0",
        "enlightenment_remote -desktop-show 3 0",
        "enlightenment_remote -desktop-show 4 0",
        "mpc toggle",
        "",
        "",
        "",
        "",
        "bash -c 'echo KeyStr Escape | xmacroplay'",
        "enlightenment_remote -desktop-lock",
        "",
        "",
    };

public:
    NagaDaemon()
    {
        size = sizeof(struct input_event);
        //Setup check
        for (auto &device : devices) {
            try {
                RaiiFd side_btn(device.first, O_RDONLY);
                RaiiFd extra_btn(device.second, O_RDONLY);

                this->side_btn = make_unique<RaiiFd>(move(side_btn));
                this->extra_btn = make_unique<RaiiFd>(move(extra_btn));
            } catch (const std::system_error &error) {
                if (error.code().value() == ENOENT)
                    continue;

                cout << "Reading from: " << device.first << " and "
                     << device.second << '\n' << "Error: " << error.code()
                     << " - " << error.code().message() << endl;
            }
        }
        if (!side_btn || !extra_btn) {
            cerr << "No naga devices found or you don't have permission to access them."
                 << endl;
            exit(EXIT_FAILURE);
        }
    }

    void run() {
        int rd, rd1, rd2;
        fd_set readset;

        while (true) {
            FD_ZERO(&readset);
            FD_SET(side_btn->fd, &readset);
            FD_SET(extra_btn->fd, &readset);
            rd = select(FD_SETSIZE, &readset, NULL, NULL, NULL);
            if (rd == -1) throw system_error(errno, system_category());

            if (FD_ISSET(side_btn->fd, &readset)) { // Side buttons
                {
                    rd1 = read(side_btn->fd, ev1, size * 64);
                    if (rd1 == -1) throw system_error(errno, system_category());

                    // Only read the key press event
                    if (ev1[0].value != ' ' && ev1[1].value == 1
                        && ev1[1].type == 1) {
                        switch (ev1[1].code) {
                        case 2:
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                        case 8:
                        case 9:
                        case 10:
                        case 11:
                        case 12:
                        case 13:
                            chooseAction(ev1[1].code - 2);
                            break;
                            // do nothing on default
                        }
                    }

                }
            } else { // Extra buttons
                rd2 = read(extra_btn->fd, ev2, size * 64);
                if (rd2 == -1) throw system_error(errno, system_category());

                // Only extra buttons
                if (ev2[1].type == 1 && ev2[1].value == 1) {
                    switch (ev2[1].code) {
                    case 275:
                    case 276:
                        chooseAction(ev2[1].code - OFFSET);
                        break;
                        // do nothing on default
                    }
                }
            }
        }
    }

    void chooseAction(int i) {
        int pid = system(args[i].c_str());
    }
};


int main()
{
    NagaDaemon daemon;
    clog << "Starting naga daemon" << endl;
    try {
        daemon.run();
    } catch (const std::system_error &error) {
        cout  << "Error: " << error.code() << " - " << error.code().message()
              << endl;
        exit(EXIT_FAILURE);
    }

    return 0;
}
