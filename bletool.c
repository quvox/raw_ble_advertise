/**
 * BLE advertise and scan tool
 *  t-kubo @ Zettant Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <bluetooth.h>
#include <hci.h>
#include <hci_lib.h>

#define DEVICE_NAME   "hci0"
#define DEFAULT_ADV_HEADER "1F0201060303AAFE1716AAFE80"

static struct hci_filter ofilter;

static volatile int signal_received = 0;

static void sigint_handler(int sig) {
    signal_received = sig;
}


static void hex_dump(char *pref, int width, unsigned char *buf, int len)
{
    register int i,n;

    for (i = 0, n = 1; i < len; i++, n++) {
        if (n == 1)
            printf("%s", pref);
        printf("%2.2X ", buf[i]);
        if (n == width) {
            printf("\n");
            n = 0;
        }
    }
    if (i && n!=1)
        printf("\n");
}

static int open_device(char *dev_name)
{
    int dev_id = hci_devid(dev_name);
    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    int dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("Could not open device");
        exit(1);
    }
    return dd;
}

void ctrl_command(uint8_t ogf, uint16_t ocf, char *data) {
    unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr = buf, tmp[2];
    struct hci_filter flt;
    int i, len, dd;

    dd = open_device(DEVICE_NAME);
    len = (int)(strlen(data)/2);
    
    for (i=0; i<len; i++) {
        memcpy(tmp, &data[i*2], 2);
        *ptr++ = (uint8_t) strtol((const char *)tmp, NULL, 16);
    }
    
    /* Setup filter */
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_all_events(&flt);
    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        hci_close_dev(dd);
        perror("HCI filter setup failed");
        exit(EXIT_FAILURE);
    }

    if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0) {
        hci_close_dev(dd);
        perror("Send failed");
        exit(EXIT_FAILURE);
    }
    hci_close_dev(dd);
}

void configure(uint16_t min_interval, uint16_t max_interval)
{
    char data[31];
    sprintf(data, "%04X%04X0000000000000000000700", htons(min_interval), htons(max_interval));
    ctrl_command(0x08, 0x0006, data);
}

void advertise_on()
{
    ctrl_command(0x08, 0x000a, "01");
}

void advertise_off()
{
    ctrl_command(0x08, 0x000a, "00");
}

void set_advertisement_data(char *data)
{
    int i;
    char alldata[64];
    sprintf(alldata, "%s%s", DEFAULT_ADV_HEADER, data);
    for (i=strlen(alldata); i<64; i++) {
        alldata[i] = '0';
    }
    ctrl_command(0x08, 0x0008, alldata);
}

static u_char recvbuf[HCI_MAX_EVENT_SIZE];

int read_advertise(int dd, uint8_t *data, int datalen)
{
    int len;
    evt_le_meta_event *meta;
    le_advertising_info *info;
    unsigned char *ptr;

    while ((len = read(dd, recvbuf, sizeof(recvbuf))) < 0) {
        if (errno == EINTR && signal_received == SIGINT) {
            return 0;
        }

        if (errno == EAGAIN || errno == EINTR)
            continue;
    }

    ptr = recvbuf + (1 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);
    meta = (void *) ptr;

    info = (le_advertising_info *) (meta->data + 1);
    memcpy(data, info->data, datalen);
    return len;
}

int print_advertising_devices(int dd) {
    struct sigaction sa;
    unsigned char dat[31];

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    while (1) {
        if (read_advertise(dd, dat, 31) == 0) break;
        hex_dump("  ", 40, dat, 31);
    }
    return 0;
}


void lescan_close(int dd)
{
    uint8_t filter_dup = 0;
    if (dd == -1) {
        dd = open_device(DEVICE_NAME);
    } else {
        setsockopt(dd, SOL_HCI, HCI_FILTER, &ofilter, sizeof(ofilter));
    }
    int err = hci_le_set_scan_enable(dd, 0x00, filter_dup, 1000);
    if (err < 0) {
        perror("Disable scan failed");
        exit(1);
    }
    hci_close_dev(dd);
}

int lescan_setup() {
    int err, dd;
    uint8_t own_type = 0x00;
    uint8_t scan_type = 0x00; // passive
    uint8_t filter_policy = 0x00;
    uint16_t interval = htobs(0x0010);
    uint16_t window = htobs(0x0010);
    uint8_t filter_dup = 0;

    dd = open_device(DEVICE_NAME);

    err = hci_le_set_scan_parameters(dd, scan_type, interval, window,
                                     own_type, filter_policy, 1000);
    if (err < 0) {
        lescan_close(-1);
        perror("Set scan parameters failed");
        exit(1);
    }

    err = hci_le_set_scan_enable(dd, 0x01, filter_dup, 1000);
    if (err < 0) {
        hci_close_dev(dd);
        perror("Enable scan failed");
        exit(1);
    }
    
    struct hci_filter nf;
    socklen_t olen;

    olen = sizeof(ofilter);
    if (getsockopt(dd, SOL_HCI, HCI_FILTER, &ofilter, &olen) < 0) {
        hci_close_dev(dd);
        printf("Could not get socket options\n");
        return -1;
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);

    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
        printf("Could not set socket options\n");
        return -1;
    }

    return dd;
}


static void usage(void)
{
    printf("bletool\n");
    printf("Usage:\n"
           "\tbletool [options] [hex_string to send]\n");
    printf("Options:\n"
           "\t--help\tDisplay help\n"
           "\t-r\tReceive mode\n"
           "\t-s hex_string\tSend data mode\n");
}

static struct option main_options[] = {
    { "help",	0, 0, 'h' },
    { "read",	1, 0, 'r' },
    { "send",   1, 0, 's' },
    { 0, 0, 0, 0 }
};

int main(int argc, char **argv) {
    int mode = 1, opt;
    char *send_data;

    while ((opt=getopt_long(argc, argv, "r+s:h", main_options, NULL)) != -1) {
        switch (opt) {
        case 'r':
            mode = 1; // receive mode
            break;

        case 's':
            mode = 2;
            send_data = optarg;
            break;

        case 'h':
        default:
            usage();
            exit(0);
        }
    }

    if (mode == 1) {
        int dd = lescan_setup();
        print_advertising_devices(dd);
        lescan_close(dd);
    } else {
        configure(32, 64);
        set_advertisement_data(send_data);
        advertise_on();
        sleep(1);
        advertise_off();
    }
}
