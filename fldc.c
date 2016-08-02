/*
 *
 * SHARP fldr mode
 *     tewilove@gmail.com, All rights reserved   
 *
 */

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <errno.h>
#ifdef __APPLE__
#include <sys/malloc.h>
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

#define USB_VID 0x04dd
#define USB_PID 0x933a

#define FLDR_NAME_SIZE 8

#define FLDR_FLAG_SEND_SEC 0x00000001
#define FLDR_FLAG_HIGH_ADDR 0x00000002
#define FLDR_FLAG_DUMP 0x00000004

// F9225D50E9D244601192A03B511F80C1
static char g_sec_rec[16];

static libusb_context *g_ctx;

static void dump(const char *head, const void *data, size_t size) {
    size_t i;

    fprintf(stderr, "%s\n", head);
    fflush(stderr);
    for (i = 0; i < size; i++) {
        fprintf(stderr, "%02x ", *((char *) data + i) & 0xff);
        if ((i + 1) % 16 == 0)
            fprintf(stderr, "\n");
    }
    if (i % 16)
        fprintf(stderr, "\n");
    fflush(stderr);
}

static int file_read_all(const char *path, char **data, size_t *size) {
    int rc, fd;
    struct stat fs;
    size_t nbrd;

    rc = stat(path, &fs);
    if (rc)
        return rc;
    if (fs.st_size <= 0)
        return -1;
    *size = fs.st_size;
    *data = malloc(fs.st_size);
    if (*data == NULL)
        return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(*data);
        *data = NULL;
        return -1;
    }
    nbrd = 0;
    while (nbrd < fs.st_size) {
        int tmp;

        tmp = read(fd, *data + nbrd, fs.st_size - nbrd);
        if (tmp < 0) {
            if (errno == -EINTR)
                continue;
            break;
        }
        nbrd += tmp;
    }
    close(fd);
    if (nbrd < fs.st_size) {
        free(*data);
        *data = NULL;
        return -1;
    }

    return 0;
}

static int file_write_all(const char *path, const char *data, size_t size) {
    int rc, fd;
    size_t nbwr;

    fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0)
        return -1;
    nbwr = 0;
    rc = ftruncate(fd, nbwr);
    if (rc)
        goto fail_truncate;
    while (nbwr < size) {
        int tmp;

        tmp = write(fd, data + nbwr, size - nbwr);
        if (tmp < 0) {
            if (errno == -EINTR)
                continue;
            break;
        }
        nbwr += tmp;
    }
fail_truncate:
    close(fd);

    return nbwr == size ? 0 : -1;
}

static int fldr_init() {
    int rc;

    rc = libusb_init(&g_ctx);
    return rc;
}

static void fldr_free() {
    libusb_exit(g_ctx);
}

// see lsusb -v -d 04dd:933a
static libusb_device_handle *fldr_open() {
    int rc;
    libusb_device_handle *h;

    h = libusb_open_device_with_vid_pid(g_ctx, USB_VID, USB_PID);
    if (!h)
        return NULL;
    rc = libusb_claim_interface(h, 1);
    if (rc) {
        // fprintf(stderr, "%04d:%s: %s(%d)\n", __LINE__, "libusb_claim_interface", libusb_error_name(rc), rc);
        goto fail;
    }
    rc = libusb_set_interface_alt_setting(h, 1, 1);
    if (rc) {
        // fprintf(stderr, "%04d:%s: %s(%d)\n", __LINE__, "libusb_set_interface_alt_setting", libusb_error_name(rc), rc);
        goto fail;
    }
    return h;
fail:
    libusb_close(h);
    return NULL;
}

static int fldr_close(libusb_device_handle *h) {
    int rc;

    rc = libusb_release_interface(h, 1);
    libusb_close(h);
    return rc;
}

size_t fldr_read(libusb_device_handle *h, char *buff, size_t size) {
    int rc;
    size_t nbtr = 0;
    libusb_device *d;
    int ps;

    d = libusb_get_device(h);
    if (!d)
        return (size_t)(-1);
    ps = libusb_get_max_packet_size(d, 0x81);
    if (ps <= 0)
        ps = 512;
    while (nbtr < size) {
        int nb, ch;

        ch = size - nbtr;
        if (ch > ps)
            ch = ps;
        rc = libusb_bulk_transfer(
            h,
            0x81,
            (unsigned char *) buff + nbtr,
            ch,
            &nb,
            1000);
        if (rc) {
            // fprintf(stderr, "%04d:%s: %s(%d)\n", __LINE__, __func__, libusb_error_name(rc), rc);
            break;
        }
        nbtr += nb;
    }
    // fprintf(stderr, "%s: %d\n", __func__, (int) nbtr);

    return nbtr;
}

size_t fldr_write(libusb_device_handle *h, const char *buff, size_t size) {
    int rc;
    size_t nbtr = 0;
    libusb_device *d;
    int ps;

    d = libusb_get_device(h);
    if (!d)
        return (size_t)(-1);
    ps = libusb_get_max_packet_size(d, 0x01);
    if (ps <= 0)
        ps = 512;
    while (nbtr < size) {
        int nb, ch;

        ch = size - nbtr;
        if (ch > ps)
            ch = ps;
        rc = libusb_bulk_transfer(
            h,
            0x01,
            (unsigned char *) buff + nbtr,
            ch,
            &nb,
            1000);
        if (rc) {
            // fprintf(stderr, "%04d:%s: %s(%d)\n", __LINE__, __func__, libusb_error_name(rc), rc);
            break;
        }
        nbtr += nb;
    }
    // fprintf(stderr, "%s: %d\n", __func__, (int) nbtr);

    return nbtr;
}

static int fldr_get_name(struct libusb_device_handle *h, char *out) {
    size_t nbtr;
    char data[16];

    data[0] = 0x30;
    data[1] = 0x01;
    data[2] = 0xce;
    nbtr = fldr_write(h, (const char *) data, 3);
    if (nbtr != 3)
        return -1;
    nbtr = fldr_read(h, data, 11);
    /*
     * 0x31 0x09 (8 byte) sum
     */
    if (nbtr != 11 || data[0] != 0x31 || data[1] != 0x09)
        return -1;
    if (out) {
        memset(out, 0, FLDR_NAME_SIZE);
        memcpy(out, data + 2, FLDR_NAME_SIZE);
    }
    return 0;
}

static uint32_t fldr_boot(struct libusb_device_handle *h, const char *data, size_t size, int flags) {
    uint32_t ret = (uint32_t) -1;
    size_t i, nbtr;
    unsigned char head[6];
    unsigned char sum;
    uint32_t status;

    if (size > 0x4000007)
        return ret;
    /*
     * op: 1 byte
     * size: 4 byte
     * addr: 1 byte
     * data: size byte(s)
     * sum: 1 byte
     */
    head[0] = 0;
    *((uint32_t *)(head + 1)) = htonl(size + 2);
    if (flags & FLDR_FLAG_HIGH_ADDR)
        head[5] = 0xff;
    else
        head[5] = 0;
    nbtr = fldr_write(h, (const char *) head, sizeof(head));
    if (nbtr != sizeof(head))
        return ret;
    nbtr = fldr_write(h, data, size);
    if (nbtr != size)
        return ret;
    sum = 0;
    for (i = 0; i < 6; i++)
        sum += head[i];
    for (i = 0; i < size; i++)
        sum += (unsigned) data[i];
    sum = ~sum;
    nbtr = fldr_write(h, (const char *) &sum, sizeof(sum));
    if (nbtr != sizeof(sum))
        return ret;
    nbtr = fldr_read(h, (char *) &status, sizeof(status));
    if (nbtr != sizeof(status))
        return ret;
    ret = status;

    return ret;
}

int main(int argc, char *argv[]) {
    int rc, ret = -1, i, len;
    uint32_t status;
    struct libusb_device_handle *h;
    char fldr_name[FLDR_NAME_SIZE];
    char ch;
    int flags = 0;
    size_t nbtr;
    char *ifile = NULL;
    char *data = NULL;
    size_t size = 0;
    char *extra_data = NULL;
    uint32_t extra_size = 0;
    char *dfile = NULL;

    while ((ch = getopt(argc, argv, "hs:f:H:do:")) != -1) {
        switch (ch) {
            case 's': {
                char digit;

                len = strlen(optarg);
                if (len != 32)
                    goto fail_usage;
                for (i = 0; i < len; i++) {
                    digit = optarg[i];
                    if (digit >= '0' && digit <= '9')
                        digit -= '0';
                    else if (digit >= 'A' && digit <= 'F')
                        digit -= ('A' - 10);
                    else if (digit >= 'a' && digit <= 'f')
                        digit -= ('a' - 10);
                    else
                        goto fail_usage;
                    g_sec_rec[i / 2] <<= 4;
                    g_sec_rec[i / 2] |= digit;
                }
                flags |= FLDR_FLAG_SEND_SEC;
                break;
            }
            case 'f':
                ifile = optarg;
                break;
            case 'H':
                flags |= FLDR_FLAG_HIGH_ADDR;
                break;
            case 'd':
                flags |= FLDR_FLAG_DUMP;
                break;
            case 'o':
                flags |= FLDR_FLAG_DUMP;
                dfile = optarg;
                break;
            case 'h':
            default:
fail_usage:
                fprintf(stderr, "Usage:\n%s [-H] [-s <16-byte-hex>] [-f <file>]\n", argv[0]);
                fprintf(stderr, "Example:\n%s -f SHL23.ldr\n", argv[0]);
                return ret;
        }
    }
    if (ifile != NULL) {
        rc = file_read_all(ifile, &data, &size);
        if (rc < 0)
            goto fail_file_read_all;
    }
    rc = fldr_init();
    if (rc)
        goto fail_fldr_init;
    h = fldr_open();
    if (!h)
        goto fail_fldr_open;
    rc = fldr_get_name(h, fldr_name);
    if (rc)
        goto fail_fldr_get_name;
    fprintf(stdout, "Detected device: %s\n", fldr_name);
    if (flags & FLDR_FLAG_SEND_SEC) {
        nbtr = fldr_write(h, g_sec_rec, sizeof(g_sec_rec));
        if (nbtr != sizeof(g_sec_rec))
            goto fail_fldr_write;
    }
    if (data && size) {
        status = fldr_boot(h, (const char *) data, size, flags);
        ret = status == (uint32_t) 0xfc000201 ? 0 : -1;
        fprintf(stdout, "Download result: %s(%08X)\n",
            ret ? "NGNG" : "OKOK", status);
        if (!ret && (flags & FLDR_FLAG_DUMP)) {
#if 0
            size_t nb = 0, ch;
#else
            size_t nb;
#endif

            nbtr = fldr_read(h, (char *) &extra_size, sizeof(extra_size));
            if (nbtr != sizeof(extra_size) || !nbtr) {
                fprintf(stderr, "No extra data received.\n");
                goto fail_empty_data;
            }
            fprintf(stderr, "Extra size: %08x\n", (int) extra_size);
            extra_data = malloc(extra_size);
            if (!extra_data)
                goto fail_malloc;
#if 0
            while (nb < extra_size) {
                ch = extra_size - nb;
                if (ch > 512)
                    ch = 512;
                nbtr = fldr_read(h, extra_data + nb, ch);
                if (nbtr == 0 || nbtr == (size_t) -1)
                    break;
                nb += nbtr;
            }
#else
            nb = fldr_read(h, extra_data, extra_size);
            if (nb == (size_t) -1)
                nb = 0;
#endif
            if (!dfile) {
                dump("Extra data:", extra_data, nb);
            } else {
                file_write_all(dfile, extra_data, nb);
            }
            free(extra_data);
        }
    } else {
        ret = 0;
    }
fail_malloc:
fail_empty_data:
fail_fldr_write:
fail_fldr_get_name:
    fldr_close(h);
fail_fldr_open:
    fldr_free();
fail_fldr_init:
    if (data && size)
        free(data);
fail_file_read_all:
    return ret;
}
