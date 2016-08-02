    /*
     * data:
     *   IV: 16 bytes
     *   AESed Data(
     *    data:
     *     data:
     *      info: offset 0x200,24
     *                   0x220,240
     *      name: offset 0x400,8(DL50, etc)
     *      ...
     *      code(with nkb): offset 0x800
     *     sum: 4
     *     padding: 16(=0xffffffff) if sum == -1
     *    sum: 4 

                 * author : tewilove
                 * saver  : nilaomudeji

     *   )
*/

int check_model(char *a) {
    return memcmp(a, "DL50", 4);
}
static int func1(int a, int b, int *c) {
    char data[12];
    char buff[4];
    int n;
    sprintf(data, "%04u", a);
    n = log40(a);
    if (n >= 4) {
        buff[0] = data[n - 4] ^ *((char *) &b + 0);
        buff[1] = data[n - 3] ^ *((char *) &b + 1);
        buff[2] = data[n - 2] ^ *((char *) &b + 2);
        buff[3] = data[n - 1] ^ *((char *) &b + 3);
    } else {
        buff[0] = data[0] ^ *((char *) &b + 0);
        buff[1] = data[1] ^ *((char *) &b + 1);
        buff[2] = data[2] ^ *((char *) &b + 2);
        buff[3] = data[3] ^ *((char *) &b + 3);
    }
    return memcmp(buff, c, 4);
}
static int func2(int a, int b, int *c) {
    char data[12];
    char *buff = (char *) c;
    int n;
    sprintf(data, "%04u", a);
    n = log40(a);
    if (n >= 4) {
        buff[0] = data[n - 4] ^ *((char *) &b + 0);
        buff[1] = data[n - 3] ^ *((char *) &b + 1);
        buff[2] = data[n - 2] ^ *((char *) &b + 2);
        buff[3] = data[n - 1] ^ *((char *) &b + 3);
    } else {
        buff[0] = data[0] ^ *((char *) &b + 0);
        buff[1] = data[1] ^ *((char *) &b + 1);
        buff[2] = data[2] ^ *((char *) &b + 2);
        buff[3] = data[3] ^ *((char *) &b + 3);
    }
}
// d1 = off + 544
// d2 = off + 512, 24
// nm = DL50\0\0\0\0\0\0
int check_info(int *d1, int *d2, int *nm) {
    int var1[15];
    int var2[15];
    int rc, i;
    for (i = 1; i < 16; i++) {
        rc = func1(d2[0] * d2[5] * i, d1[8 * i + 0], nm);
        if (rc)
            return 2;
        rc = func1(d2[2] * d2[5] * i, d1[8 * i + 1], nm + 1);
        if (rc)
            return 2;
        func2(d2[1] * d2[5] * i, d1[8 * i + 6], var1 + i - 1);
        func2(d2[3] * d2[5] * i, d1[8 * i + 7], var2 + i - 1);
    }
    for (i = 1; i < 15; i++) {
        if (memcmp(var1, var1 + i, 4))
            return 1;
        if (memcmp(var2, var2 + i, 4))
            return 1;
    }
    return 0;
}
// after 2k offset
// every 1kb data, data[0] = data[]
int check_nkb(char *d, int s) {
    int i;
    int n = s >> 10;
    for (i = 2; i < n; i++) {
        if (d[i *1024] != d[i * 1022])
            return 1;
    }
    return 0;
}
int fixup_nkb(char *data, int size) {
    int i, n, tail, nk, left;
    n = 0;
    tail = size - 4;
    memset(data + size - 4, 0xFF, 4);
    nk = tail / 1024;
    left = tail & 1023;
    for (i = nk; i >= 2; i--) {
        if (i == nb && left) {
            n = left - 1;
            memcpy(data + i * 1024, data + i * 1024 + 1, n);
            tail -= 1;
        } else {
            n += 1023;
            memcpy(data + i * 1024, data + i * 1024 + 1, n);
            tail -= 1;
        }
    }
    memcpy(data + 512, data + 1024, tail + 1024);
    return tail - 512;
}
