// mxp, 20211105, this file is copied from system/core/toolbox
// we capture event from capture_devices, current is : gpio_keys_polled & adc-keys
// and we convert the keyeven to new keyevent
// we map do the following map
// KEY_DOWN  -> KEY_UP
// KEY_UP    -> KEY_DOWN
// KEY_POWER -> KEY_SELECT

// and support multi key press
// KEY_UP   + KEY_DOWN    -> KEY_BACK
// KEY_UP   + KEY_SELECT  -> KEY_LEFT
// KEY_DOWN + KEY_SELECT  -> KEY_RIGHT

#define LOG_TAG "frontpaneld"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <errno.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <utils/Log.h>
#include <map>
#include "frontpaneld.h"

#define UINPUT_DEVICE_NAME "frontpaneld"
// mxp, 20211109, do not use 500ms, because it's android default repeate key timeout
#define COMBINATION_KEY_TIMEOUT_MS (400)
#define MAX_PENDING_EVENTS (5)

static struct pollfd *ufds;
static char **device_names;
static int nfds;

static int pending_event_num = 0;
static struct input_event pending_events[MAX_PENDING_EVENTS];
static bool comb_key_is_down = false;
static int keycode_in_dispatching = -1;

static const char* capture_devices[] = {
  "gpio_keys_polled",
  "adc-keys"
};

// mxp, 20211104, hardware keycode map
// receive & report with new keycode
static struct {
  int key;
  int val;
} keycode_convert_table[] {
  {KEY_DOWN, KEY_UP},
  {KEY_UP, KEY_DOWN},
  {KEY_POWER, KEY_SELECT}
};

// mxp, 20211104, we supported keycode list
static int supported_keycode[] = {
  KEY_SELECT, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_BACK
};

// mxp, 20211104, map two keys to new key
static struct {
  int key_1;
  int key_2;
  int value;
} combinative_keycode_table[] {
  // UP + DOWN = BACK
  {KEY_UP, KEY_DOWN, KEY_BACK},
  {KEY_DOWN, KEY_UP, KEY_BACK},
  // UP + SELECT = LEFT
  {KEY_UP, KEY_SELECT, KEY_LEFT},
  {KEY_SELECT, KEY_UP, KEY_LEFT},
  // DOWN + SELECT = RIGHT
  {KEY_DOWN, KEY_SELECT, KEY_RIGHT},
  {KEY_SELECT, KEY_DOWN, KEY_RIGHT},
};

enum {
    PRINT_DEVICE_ERRORS     = 1U << 0,
    PRINT_DEVICE            = 1U << 1,
    PRINT_DEVICE_NAME       = 1U << 2,
    PRINT_DEVICE_INFO       = 1U << 3,
    PRINT_VERSION           = 1U << 4,
    PRINT_POSSIBLE_EVENTS   = 1U << 5,
    PRINT_INPUT_PROPS       = 1U << 6,
    PRINT_HID_DESCRIPTOR    = 1U << 7,

    PRINT_ALL_INFO          = (1U << 8) - 1,

    PRINT_LABELS            = 1U << 16,
};

static bool should_capture_devices(const char* name);
static int uinput_trigger_keyevent(int fd, struct input_event ev);
static int uinput_trigger_keyevent(int fd, int keycode, int state);
static int probe_combination_keycode(struct input_event *ev);
static int mapped_keycode(int keycode);
static int uinput_device_init(void);

static const char *get_label(const struct label *labels, int value)
{
    while(labels->name && value != labels->value) {
        labels++;
    }
    return labels->name;
}

static int print_input_props(int fd)
{
    uint8_t bits[INPUT_PROP_CNT / 8];
    int i, j;
    int res;
    int count;
    const char *bit_label;

    printf("  input props:\n");
    res = ioctl(fd, EVIOCGPROP(sizeof(bits)), bits);
    if(res < 0) {
        printf("    <not available\n");
        return 1;
    }
    count = 0;
    for(i = 0; i < res; i++) {
        for(j = 0; j < 8; j++) {
            if (bits[i] & 1 << j) {
                bit_label = get_label(input_prop_labels, i * 8 + j);
                if(bit_label)
                    printf("    %s\n", bit_label);
                else
                    printf("    %04x\n", i * 8 + j);
                count++;
            }
        }
    }
    if (!count)
        printf("    <none>\n");
    return 0;
}

static int print_possible_events(int fd, int print_flags)
{
    uint8_t *bits = NULL;
    ssize_t bits_size = 0;
    const char* label;
    int i, j, k;
    int res, res2;
    struct label* bit_labels;
    const char *bit_label;

    printf("  events:\n");
    for(i = EV_KEY; i <= EV_MAX; i++) { // skip EV_SYN since we cannot query its available codes
        int count = 0;
        while(1) {
            res = ioctl(fd, EVIOCGBIT(i, bits_size), bits);
            if(res < bits_size)
                break;
            bits_size = res + 16;
            bits = (uint8_t *)realloc(bits, bits_size * 2);
            if(bits == NULL) {
                fprintf(stderr, "failed to allocate buffer of size %d\n", (int)bits_size);
                return 1;
            }
        }
        res2 = 0;
        switch(i) {
            case EV_KEY:
                res2 = ioctl(fd, EVIOCGKEY(res), bits + bits_size);
                label = "KEY";
                bit_labels = key_labels;
                break;
            case EV_REL:
                label = "REL";
                bit_labels = rel_labels;
                break;
            case EV_ABS:
                label = "ABS";
                bit_labels = abs_labels;
                break;
            case EV_MSC:
                label = "MSC";
                bit_labels = msc_labels;
                break;
            case EV_LED:
                res2 = ioctl(fd, EVIOCGLED(res), bits + bits_size);
                label = "LED";
                bit_labels = led_labels;
                break;
            case EV_SND:
                res2 = ioctl(fd, EVIOCGSND(res), bits + bits_size);
                label = "SND";
                bit_labels = snd_labels;
                break;
            case EV_SW:
                res2 = ioctl(fd, EVIOCGSW(bits_size), bits + bits_size);
                label = "SW ";
                bit_labels = sw_labels;
                break;
            case EV_REP:
                label = "REP";
                bit_labels = rep_labels;
                break;
            case EV_FF:
                label = "FF ";
                bit_labels = ff_labels;
                break;
            case EV_PWR:
                label = "PWR";
                bit_labels = NULL;
                break;
            case EV_FF_STATUS:
                label = "FFS";
                bit_labels = ff_status_labels;
                break;
            default:
                res2 = 0;
                label = "???";
                bit_labels = NULL;
        }
        for(j = 0; j < res; j++) {
            for(k = 0; k < 8; k++)
                if(bits[j] & 1 << k) {
                    char down;
                    if(j < res2 && (bits[j + bits_size] & 1 << k))
                        down = '*';
                    else
                        down = ' ';
                    if(count == 0)
                        printf("    %s (%04x):", label, i);
                    else if((count & (print_flags & PRINT_LABELS ? 0x3 : 0x7)) == 0 || i == EV_ABS)
                        printf("\n               ");
                    if(bit_labels && (print_flags & PRINT_LABELS)) {
                        bit_label = get_label(bit_labels, j * 8 + k);
                        if(bit_label)
                            printf(" %.20s%c%*s", bit_label, down, 20 - strlen(bit_label), "");
                        else
                            printf(" %04x%c                ", j * 8 + k, down);
                    } else {
                        printf(" %04x%c", j * 8 + k, down);
                    }
                    if(i == EV_ABS) {
                        struct input_absinfo abs;
                        if(ioctl(fd, EVIOCGABS(j * 8 + k), &abs) == 0) {
                            printf(" : value %d, min %d, max %d, fuzz %d, flat %d, resolution %d",
                                abs.value, abs.minimum, abs.maximum, abs.fuzz, abs.flat,
                                abs.resolution);
                        }
                    }
                    count++;
                }
        }
        if(count)
            printf("\n");
    }
    free(bits);
    return 0;
}

static void print_event(int type, int code, int value, int print_flags)
{
    const char *type_label, *code_label, *value_label;

    if (print_flags & PRINT_LABELS) {
        type_label = get_label(ev_labels, type);
        code_label = NULL;
        value_label = NULL;

        switch(type) {
            case EV_SYN:
                code_label = get_label(syn_labels, code);
                break;
            case EV_KEY:
                code_label = get_label(key_labels, code);
                value_label = get_label(key_value_labels, value);
                break;
            case EV_REL:
                code_label = get_label(rel_labels, code);
                break;
            case EV_ABS:
                code_label = get_label(abs_labels, code);
                switch(code) {
                    case ABS_MT_TOOL_TYPE:
                        value_label = get_label(mt_tool_labels, value);
                }
                break;
            case EV_MSC:
                code_label = get_label(msc_labels, code);
                break;
            case EV_LED:
                code_label = get_label(led_labels, code);
                break;
            case EV_SND:
                code_label = get_label(snd_labels, code);
                break;
            case EV_SW:
                code_label = get_label(sw_labels, code);
                break;
            case EV_REP:
                code_label = get_label(rep_labels, code);
                break;
            case EV_FF:
                code_label = get_label(ff_labels, code);
                break;
            case EV_FF_STATUS:
                code_label = get_label(ff_status_labels, code);
                break;
        }

        if (type_label)
            printf("%-12.12s", type_label);
        else
            printf("%04x        ", type);
        if (code_label)
            printf(" %-20.20s", code_label);
        else
            printf(" %04x                ", code);
        if (value_label)
            printf(" %-20.20s", value_label);
        else
            printf(" %08x            ", value);
    } else {
        printf("%04x %04x %08x", type, code, value);
    }
}

static void print_hid_descriptor(int bus, int vendor, int product)
{
    const char *dirname = "/sys/kernel/debug/hid";
    char prefix[16];
    DIR *dir;
    struct dirent *de;
    char filename[PATH_MAX];
    FILE *file;
    char line[2048];

    snprintf(prefix, sizeof(prefix), "%04X:%04X:%04X.", bus, vendor, product);

    dir = opendir(dirname);
    if(dir == NULL)
        return;
    while((de = readdir(dir))) {
        if (strstr(de->d_name, prefix) == de->d_name) {
            snprintf(filename, sizeof(filename), "%s/%s/rdesc", dirname, de->d_name);

            file = fopen(filename, "r");
            if (file) {
                printf("  HID descriptor: %s\n\n", de->d_name);
                while (fgets(line, sizeof(line), file)) {
                    fputs("    ", stdout);
                    fputs(line, stdout);
                }
                fclose(file);
                puts("");
            }
        }
    }
    closedir(dir);
}

static int open_device(const char *device, int print_flags)
{
    int version;
    int fd;
    struct pollfd *new_ufds;
    char **new_device_names;
    char name[80];
    char location[80];
    char idstr[80];
    struct input_id id;

    fd = open(device, O_RDWR);
    if(fd < 0) {
        if(print_flags & PRINT_DEVICE_ERRORS)
            fprintf(stderr, "could not open %s, %s\n", device, strerror(errno));
        return -1;
    }
    
    if(ioctl(fd, EVIOCGVERSION, &version)) {
        if(print_flags & PRINT_DEVICE_ERRORS)
            fprintf(stderr, "could not get driver version for %s, %s\n", device, strerror(errno));
        return -1;
    }
    if(ioctl(fd, EVIOCGID, &id)) {
        if(print_flags & PRINT_DEVICE_ERRORS)
            fprintf(stderr, "could not get driver id for %s, %s\n", device, strerror(errno));
        return -1;
    }
    name[sizeof(name) - 1] = '\0';
    location[sizeof(location) - 1] = '\0';
    idstr[sizeof(idstr) - 1] = '\0';
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        //fprintf(stderr, "could not get device name for %s, %s\n", device, strerror(errno));
        name[0] = '\0';
    }
    if(ioctl(fd, EVIOCGPHYS(sizeof(location) - 1), &location) < 1) {
        //fprintf(stderr, "could not get location for %s, %s\n", device, strerror(errno));
        location[0] = '\0';
    }
    if(ioctl(fd, EVIOCGUNIQ(sizeof(idstr) - 1), &idstr) < 1) {
        //fprintf(stderr, "could not get idstring for %s, %s\n", device, strerror(errno));
        idstr[0] = '\0';
    }

    printf("device name:%s\n", name);

    if (!should_capture_devices(name)) {
      close(fd);
      return 0;
    }

    new_ufds = (struct pollfd*)realloc(ufds, sizeof(ufds[0]) * (nfds + 1));
    if(new_ufds == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }
    ufds = new_ufds;
    new_device_names = (char **)realloc(device_names, sizeof(device_names[0]) * (nfds + 1));
    if(new_device_names == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }
    device_names = new_device_names;

    if(print_flags & PRINT_DEVICE)
        printf("add device %d: %s\n", nfds, device);
    if(print_flags & PRINT_DEVICE_INFO)
        printf("  bus:      %04x\n"
               "  vendor    %04x\n"
               "  product   %04x\n"
               "  version   %04x\n",
               id.bustype, id.vendor, id.product, id.version);
    if(print_flags & PRINT_DEVICE_NAME)
        printf("  name:     \"%s\"\n", name);
    if(print_flags & PRINT_DEVICE_INFO)
        printf("  location: \"%s\"\n"
               "  id:       \"%s\"\n", location, idstr);
    if(print_flags & PRINT_VERSION)
        printf("  version:  %d.%d.%d\n",
               version >> 16, (version >> 8) & 0xff, version & 0xff);

    if(print_flags & PRINT_POSSIBLE_EVENTS) {
        print_possible_events(fd, print_flags);
    }

    if(print_flags & PRINT_INPUT_PROPS) {
        print_input_props(fd);
    }
    if(print_flags & PRINT_HID_DESCRIPTOR) {
        print_hid_descriptor(id.bustype, id.vendor, id.product);
    }

    ufds[nfds].fd = fd;
    ufds[nfds].events = POLLIN;
    device_names[nfds] = strdup(device);
    nfds++;

    return 0;
}

int close_device(const char *device, int print_flags)
{
    int i;
    for(i = 1; i < nfds; i++) {
        if(strcmp(device_names[i], device) == 0) {
            int count = nfds - i - 1;
            if(print_flags & PRINT_DEVICE)
                printf("remove device %d: %s\n", i, device);
            free(device_names[i]);
            memmove(device_names + i, device_names + i + 1, sizeof(device_names[0]) * count);
            memmove(ufds + i, ufds + i + 1, sizeof(ufds[0]) * count);
            nfds--;
            return 0;
        }
    }
    if(print_flags & PRINT_DEVICE_ERRORS)
        fprintf(stderr, "remote device: %s not found\n", device);
    return -1;
}

static int read_notify(const char *dirname, int nfd, int print_flags)
{
    int res;
    char devname[PATH_MAX];
    char *filename;
    char event_buf[512];
    int event_size;
    int event_pos = 0;
    struct inotify_event *event;

    res = read(nfd, event_buf, sizeof(event_buf));
    if(res < (int)sizeof(*event)) {
        if(errno == EINTR)
            return 0;
        fprintf(stderr, "could not get event, %s\n", strerror(errno));
        return 1;
    }
    //printf("got %d bytes of event information\n", res);

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';

    while(res >= (int)sizeof(*event)) {
        event = (struct inotify_event *)(event_buf + event_pos);
        //printf("%d: %08x \"%s\"\n", event->wd, event->mask, event->len ? event->name : "");
        if(event->len) {
            strcpy(filename, event->name);
            if(event->mask & IN_CREATE) {
                open_device(devname, print_flags);
            }
            else {
                close_device(devname, print_flags);
            }
        }
        event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }
    return 0;
}

static int scan_dir(const char *dirname, int print_flags)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        open_device(devname, print_flags);
    }
    closedir(dir);
    return 0;
}

static void usage(int argc, char *argv[])
{
    fprintf(stderr, "Usage: %s [-t] [-n] [-s switchmask] [-S] [-v [mask]] [-d] [-p] [-i] [-l] [-q] [-c count] [-r] [device]\n", argv[0]);
    fprintf(stderr, "    -t: show time stamps\n");
    fprintf(stderr, "    -n: don't print newlines\n");
    fprintf(stderr, "    -s: print switch states for given bits\n");
    fprintf(stderr, "    -S: print all switch states\n");
    fprintf(stderr, "    -v: verbosity mask (errs=1, dev=2, name=4, info=8, vers=16, pos. events=32, props=64)\n");
    fprintf(stderr, "    -d: show HID descriptor, if available\n");
    fprintf(stderr, "    -p: show possible events (errs, dev, name, pos. events)\n");
    fprintf(stderr, "    -i: show all device info and possible events\n");
    fprintf(stderr, "    -l: label event types and names in plain text\n");
    fprintf(stderr, "    -q: quiet (clear verbosity mask)\n");
    fprintf(stderr, "    -c: print given number of events then exit\n");
    fprintf(stderr, "    -r: print rate events are received\n");
}

int main(int argc, char *argv[])
{
    int c;
    int i;
    int res;
    int pollres;
    int get_time = 0;
    int print_device = 0;
    const char *newline = "\n";
    uint16_t get_switch = 0;
    struct input_event event;
    int version;
    int print_flags = 0;
    int print_flags_set = 0;
    int dont_block = -1;
    int event_count = 0;
    int sync_rate = 0;
    const char *device = NULL;
    const char *device_path = "/dev/input";

    opterr = 0;
    do {
        c = getopt(argc, argv, "tns:Sv::dpilqc:rh");
        if (c == EOF)
            break;
        switch (c) {
        case 't':
            get_time = 1;
            break;
        case 'n':
            newline = "";
            break;
        case 's':
            get_switch = strtoul(optarg, NULL, 0);
            if(dont_block == -1)
                dont_block = 1;
            break;
        case 'S':
            get_switch = ~0;
            if(dont_block == -1)
                dont_block = 1;
            break;
        case 'v':
            if(optarg)
                print_flags |= strtoul(optarg, NULL, 0);
            else
                print_flags |= PRINT_DEVICE | PRINT_DEVICE_NAME | PRINT_DEVICE_INFO | PRINT_VERSION;
            print_flags_set = 1;
            break;
        case 'd':
            print_flags |= PRINT_HID_DESCRIPTOR;
            break;
        case 'p':
            print_flags |= PRINT_DEVICE_ERRORS | PRINT_DEVICE
                    | PRINT_DEVICE_NAME | PRINT_POSSIBLE_EVENTS | PRINT_INPUT_PROPS;
            print_flags_set = 1;
            if(dont_block == -1)
                dont_block = 1;
            break;
        case 'i':
            print_flags |= PRINT_ALL_INFO;
            print_flags_set = 1;
            if(dont_block == -1)
                dont_block = 1;
            break;
        case 'l':
            print_flags |= PRINT_LABELS;
            break;
        case 'q':
            print_flags_set = 1;
            break;
        case 'c':
            event_count = atoi(optarg);
            dont_block = 0;
            break;
        case 'r':
            sync_rate = 1;
            break;
        case '?':
            fprintf(stderr, "%s: invalid option -%c\n",
                argv[0], optopt);
            [[fallthrough]];
        case 'h':
            usage(argc, argv);
            exit(1);
            break;
        }
    } while (1);
    if(dont_block == -1)
        dont_block = 0;

    if (optind + 1 == argc) {
        device = argv[optind];
        optind++;
    }
    if (optind != argc) {
        usage(argc, argv);
        exit(1);
    }

    int uinput_fd = uinput_device_init();

    nfds = 1;
    ufds = (struct pollfd *)calloc(1, sizeof(ufds[0]));
    ufds[0].fd = inotify_init();
    ufds[0].events = POLLIN;
    if(device) {
        if(!print_flags_set)
            print_flags |= PRINT_DEVICE_ERRORS;
        res = open_device(device, print_flags);
        if(res < 0) {
            return 1;
        }
    } else {
        if(!print_flags_set)
            print_flags |= PRINT_DEVICE_ERRORS | PRINT_DEVICE | PRINT_DEVICE_NAME;
        print_device = 1;
		res = inotify_add_watch(ufds[0].fd, device_path, IN_DELETE | IN_CREATE);
        if(res < 0) {
            fprintf(stderr, "could not add watch for %s, %s\n", device_path, strerror(errno));
            return 1;
        }
        res = scan_dir(device_path, print_flags);
        if(res < 0) {
            fprintf(stderr, "scan dir failed for %s\n", device_path);
            return 1;
        }
    }

    if(get_switch) {
        for(i = 1; i < nfds; i++) {
            uint16_t sw;
            res = ioctl(ufds[i].fd, EVIOCGSW(1), &sw);
            if(res < 0) {
                fprintf(stderr, "could not get switch state, %s\n", strerror(errno));
                return 1;
            }
            sw &= get_switch;
            printf("%04x%s", sw, newline);
        }
    }

    if(dont_block)
        return 0;

    int should_trigger_timeout = -1; // 500ms
    while(1) {
      pollres = poll(ufds, nfds, should_trigger_timeout);
        //printf("poll %d, returned %d, pending_event_num:%d, should_trigger_timeout:%d, comb_key_is_down:%d\n", nfds, pollres, pending_event_num, should_trigger_timeout, comb_key_is_down);
        // mxp, 20211104, timeout we should trigger pending event
        if (pollres == 0) {

          should_trigger_timeout = -1;

          // 1. we should drop invalid event
          if (pending_event_num == 0) {
            continue;
          }

          //printf("pending_event_num:%d first event: %s(%d), current key state:%d\n", pending_event_num, get_label(key_labels, pending_events[0].code), pending_events[0].value, comb_key_is_down);
          // 1.1 we are not in down state, drop the first key
          bool is_down = pending_events[0].value == 1;

          if (comb_key_is_down == is_down) {
            if (pending_events[0].value == 0) {
              //ALOGI("we state not match, we should drop first %d-%d...\n", comb_key_is_down, is_down);
              // drop the first event
              pending_event_num--;
              if (pending_event_num > 0) {
                memcpy((void*)&pending_events[0], (void*)&pending_events[1], sizeof(pending_events[0])* pending_event_num);
                // trigger immediate
                should_trigger_timeout = 0;
              }
              continue;
            }
          }

          // mxp, 20211105, user may release two keys not at same time
          // such as release a, and b is also pressed and released after 5s
          // 2 method:
          // a. wait another key released
          // b. directly release the current dispatching keycode, and drop other key release event
          // should we release key, when any key
          // the following condition means that the event is up
          if (keycode_in_dispatching != -1 && keycode_in_dispatching != event.code) {
            // we should wait other comb key release
            //ALOGW("trigger combination key release event ...\n");
            uinput_trigger_keyevent(uinput_fd, keycode_in_dispatching, 0);
            pending_event_num--;
            should_trigger_timeout = pending_event_num != 0 ? 0: -1;
            //should_trigger_timeout = -1;
            continue;
          }

          // 2. more than one key ?
          // 2.1 one key, directly dispatch it
          if (pending_event_num == 1) {
            uinput_trigger_keyevent(uinput_fd, pending_events[0]);
            should_trigger_timeout = -1;
            pending_event_num = 0;
            continue;
          } else {
            // new key ?
            struct input_event comb_event;
            memset((void*)&comb_event, 0, sizeof(comb_event));
            comb_event.type = EV_KEY;
            if ( 0 == probe_combination_keycode(&comb_event)) {
              uinput_trigger_keyevent(uinput_fd, comb_event);
              pending_event_num -= 2;
              ALOGI("comb keycode:%s, is down:%d, left events:%d\n", get_label(key_labels, comb_event.code), comb_key_is_down, pending_event_num);
              if (pending_event_num > 0) { // elements large than 2
                memcpy((void*)&pending_events[0], (void*)&pending_events[2], sizeof(pending_events[0])* pending_event_num);
              }
            } else {
              // not comb key set, dispatch once
              uinput_trigger_keyevent(uinput_fd, pending_events[0]);
              pending_event_num--;
              if (pending_event_num > 0) {
                memcpy((void*)&pending_events[0], (void*)&pending_events[1], sizeof(pending_events[0])* pending_event_num);
              }
            }
          }

          should_trigger_timeout = pending_event_num != 0 ? 0: -1;
          continue;
        }
        if (ufds[0].revents & POLLIN) {
            read_notify(device_path, ufds[0].fd, print_flags);
        }
        for(i = 1; i < nfds; i++) {
            if(ufds[i].revents) {
                if(ufds[i].revents & POLLIN) {
                    res = read(ufds[i].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)) {
                        fprintf(stderr, "could not get event\n");
                        return 1;
                    }
                    //if(get_time) {
                    //    printf("[%8ld.%06ld] ", event.time.tv_sec, event.time.tv_usec);
                    //}
                    //if(print_device)
                    //  printf("%s: ", device_names[i]);
                    //print_event(event.type, event.code, event.value, print_flags);
                    //printf("%s", newline);

                    if(event.type == EV_KEY) {
                      int keycode = mapped_keycode(event.code);
                      if (keycode != -1) {
                        event.code = keycode;
                      }
                      //printf("pending_event_num:%d, code:%d, value:%d\n", pending_event_num, event.code, event.value);
                      if (pending_event_num < MAX_PENDING_EVENTS) {
                        memcpy((void*)&pending_events[pending_event_num], (void*)&event, sizeof(struct input_event));
                        pending_event_num++;
                      }
                      should_trigger_timeout = pending_event_num > 0 ? COMBINATION_KEY_TIMEOUT_MS : -1;
                    }

                    if(event_count && --event_count == 0)
                        return 0;
                }
            }
        }
    }

    return 0;
}

static bool should_capture_devices(const char* name) {
  if (!name) {
    return false;
  }
  int i = 0;
  for (i = 0; i < sizeof(capture_devices)/sizeof(capture_devices[0]); i++) {
    if (!strcmp(capture_devices[i], name)) {
      return true;
    }
  }

  return false;
}

static int mapped_keycode(int keycode){
  int i = 0;
  for ( i = 0; i < sizeof(keycode_convert_table)/sizeof(keycode_convert_table[0]); i++) {
    if (keycode == keycode_convert_table[i].key) {
      return keycode_convert_table[i].val;
    }
  }
  return -1;
}

static int uinput_device_init(void) {
  int i = 0;
  struct uinput_user_dev uidev;

  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  int result = ioctl(fd, UI_SET_EVBIT, EV_KEY);
  if (result == -1) {
    ALOGE("LinuxUInput register_event type %d failed: %d - %s",
          EV_KEY, errno, strerror(errno));
  }

  for (i = 0; i < sizeof(supported_keycode)/sizeof(supported_keycode[0]); i++) {
    result = ioctl(fd, UI_SET_KEYBIT, supported_keycode[i]);
    if (result == -1) {
      ALOGE("LinuxUInput register_event type %d failed: %d - %s",
            supported_keycode[i], errno, strerror(errno));
    }
  }

  memset(&uidev, 0, sizeof(uidev));
  strncpy(uidev.name, UINPUT_DEVICE_NAME, UINPUT_MAX_NAME_SIZE);

  uidev.id.bustype = BUS_HOST;
  uidev.id.product = 0;
  uidev.id.vendor = 0;
  uidev.id.version = 1;
  write(fd, &uidev, sizeof(uidev));

  if (ioctl(fd, UI_DEV_CREATE) < 0) {
    ALOGE("%s(%d): can not create device\n", __FUNCTION__, __LINE__);
  }
  return fd;
}

// do not support multi key pressed as same time
static int uinput_trigger_keyevent(int fd, struct input_event ev){
    //printf("uinput_trigger_keyevent: keycode:%s, value:%d\n", get_label(key_labels, ev.code), ev.value);
      
    if (keycode_in_dispatching != -1) {
      if (ev.code != keycode_in_dispatching) {
        //printf("%s in dispatching, we should not dispatch new key:%s\n", get_label(key_labels, keycode_in_dispatching), get_label(key_labels, ev.code));
        ALOGW("%s in dispatching, we should not dispatch new key:%s\n", get_label(key_labels, keycode_in_dispatching), get_label(key_labels, ev.code));
        return -1;
      }
    }

    comb_key_is_down = ev.value == 1;

    keycode_in_dispatching = ev.value == 1 ? ev.code : -1;

    write(fd, &ev, sizeof(ev));

    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    write(fd, &ev, sizeof(ev));

    return 0;
}

static int uinput_trigger_keyevent(int fd, int keycode, int state){
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = keycode;
    ev.value = state;
    uinput_trigger_keyevent(fd, ev);

    return 0;
}

static int probe_combination_keycode(struct input_event *ev) {
  int i = 0;
  if (!ev) {
    return -1;
  }
  if (pending_event_num <= 1) {
    return -1;
  }

  //for (i = 0; i < pending_event_num; i++) {
  //  printf("pending %d: %d(%d) %s\n", i, pending_events[i].code, pending_events[i].value, get_label(key_labels, pending_events[i].code));
  //}

  if ( pending_events[0].value !=  pending_events[1].value ) {
    //printf("two event type not matched, not comb key ...\n");
    //ALOGW("%s(%d): two event type not matched, not comb key ...\n", __FUNCTION__, __LINE__);
    return -1;
  }
  for (i = 0; i < sizeof(combinative_keycode_table) / sizeof(combinative_keycode_table[0]); i++) {
    if (combinative_keycode_table[i].key_1 == pending_events[0].code && combinative_keycode_table[i].key_2 == pending_events[1].code) {
      ev->code = combinative_keycode_table[i].value;
      ev->value = pending_events[0].value;
      return 0;
    }
  }
  return -1;
}

