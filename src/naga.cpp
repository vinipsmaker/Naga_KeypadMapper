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
#include <unistd.h>
#include <linux/input.h>

#define DEV_NUM_KEYS 12
#define EXTRA_BUTTONS 2
#define OFFSET 263

using namespace std;

static constexpr auto devices = {
    // NAGA EPIC
    "/dev/input/by-id/usb-Razer_Razer_Naga_Epic-if01-event-kbd",
    // NAGA EPIC DOCK
    "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-if01-event-kbd",
    // NAGA 2014
    "/dev/input/by-id/usb-Razer_Razer_Naga_2014-if02-event-kbd",
    // NAGA MOLTEN
    "/dev/input/by-id/usb-Razer_Razer_Naga-if01-event-kbd",
    // NAGA EPIC CHROMA
    "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-if01-event-kbd",
    // NAGA EPIC CHROMA DOCK
    "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-if01-event-kbd",
    // NAGA CHROMA
    "/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-if02-event-kbd"
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
        return *this;
    }

    RaiiFd(const RaiiFd&) = delete;
    RaiiFd& operator=(const RaiiFd&) = delete;

    int fd;
};

class NagaDaemon {
    struct input_event ev1[64], ev2[64];
    int id, size;
    // because we don't have optional<T>
    unique_ptr<RaiiFd> side_btn;

    vector<string> args {
        "enlightenment_remote -desktop-show 0 0",
        "enlightenment_remote -desktop-show 1 0",
        "enlightenment_remote -desktop-show 2 0",
        "enlightenment_remote -desktop-show 3 0",
        "enlightenment_remote -desktop-show 4 0",
        "mpc toggle",
        "mpc shuffle",
        "",
        "mpc next",
        "enlightenment_remote -restart",
        "bash -c 'echo KeyStr Escape | xmacroplay'",
        "enlightenment_remote -desktop-lock",
    };

public:
    NagaDaemon()
    {
        size = sizeof(struct input_event);
        //Setup check
        for (auto &device : devices) {
            try {
                RaiiFd side_btn(device, O_RDONLY);

                this->side_btn = make_unique<RaiiFd>(move(side_btn));
            } catch (const std::system_error &error) {
                if (error.code().value() == ENOENT)
                    continue;

                cout << "Reading from: " << device << '\n' << "Error: "
                     << error.code() << " - " << error.code().message() << endl;
            }
        }
        if (!side_btn) {
            cerr << "No naga devices found or you don't have permission to access them."
                 << endl;
            exit(EXIT_FAILURE);
        }
    }

    void run() {
        while (true) {
            int rd = read(side_btn->fd, ev1, size * 64);
            if (rd == -1) throw system_error(errno, system_category());

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
    }

    void chooseAction(int i) {
        int pid = system(args[i].c_str());
        (void)(pid);
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
