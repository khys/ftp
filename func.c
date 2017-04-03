#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "myftp.h"

void data_send(int sd, int type, int code, int len, uint8_t *pld)
{
    int cnt;
    struct myftph ftph;
    uint8_t *data;

    ftph.type = type;
    ftph.code = code;
    ftph.length = len;
    data = (uint8_t *)malloc(sizeof(struct myftph) + len);
    memcpy(data, &ftph, sizeof(struct myftph));
    if (len != 0) {
        memcpy(data + sizeof(struct myftph), pld, len);
    }
    if ((cnt = send(sd, data, sizeof(struct myftph) + len, 0)) < 0) {
        perror("send");
        close(sd);
        exit(1);
    }
    /*
    printf("type: %x, code: %x, length: %d\n",
            ((struct myftph *)data)->type,
            ((struct myftph *)data)->code,
            ((struct myftph *)data)->length);
    */
    // free(data);
}

struct myftph *ftph_recv(int sd)
{
    int cnt;
    struct myftph *ftph;

    ftph = (struct myftph *)malloc(sizeof(struct myftph));
    if ((cnt = recv(sd, ftph, sizeof(struct myftph), 0)) < 0) {
        perror("recv");
        close(sd);
        exit(1);
    }
    return ftph;
}

uint8_t *pld_recv(int sd, int size)
{
    int cnt;
    uint8_t *pld;

    pld = (uint8_t *)malloc(size);
    if ((cnt = recv(sd, pld, size, 0)) < 0) {
        perror("recv");
        close(sd);
        exit(1);
    }
    return pld;
}
