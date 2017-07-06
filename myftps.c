// myFTP server

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "myftp.h"

struct type_table {
    uint8_t type;
    void (*func)(int, char []);
} typ_tbl[] = {
    {0x01, quit_exec},
    {0x02, pwd_exec},
    {0x03, cwd_exec},
    {0x04, list_exec},
    {0x05, retr_exec},
    {0x06, stor_exec},
    {0, NULL}
};

// int sd0;

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res;
    struct sockaddr_storage sin;
    int sd0, sd1, err;
    socklen_t sktlen, len;
    struct type_table *p;
    struct myftph *ftph;
    char lbuf[BUFLEN];
    pid_t pid;

    if (argc != 2 && argc != 3) {
        fprintf(stderr,
                "Usage: ./myftps <port number> [current directory]\n");
        exit(1);
    } else if (argc == 3) {
        if (chdir(argv[2]) < 0) {
            switch (errno) {
                case ENOENT:
                    fprintf(stderr, "cd: %s: No such file or directory\n",
                            argv[2]);
                    break;
                case EACCES:
                    fprintf(stderr, "%s: Permission denied\n", argv[2]);
                    break;
                default:
                    fprintf(stderr, "chdir: Undefined error\n");
                    break;
            }
            exit(1);
        }
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    if ((err = getaddrinfo(NULL, argv[1], &hints, &res)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(1);
    }
    if ((sd0 = socket(res->ai_family, res->ai_socktype,
                      res->ai_protocol)) < 0) {
        perror("socket");
        exit(1);
    }
    if (bind(sd0, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        exit(1);
    }
    freeaddrinfo(res);
    if (listen(sd0, 5) < 0) {
        perror("listen");
        exit(1);
    }

    for (;;) {
        sktlen = sizeof(struct sockaddr_storage);
        if ((sd1 = accept(sd0, (struct sockaddr *)&sin, &sktlen)) < 0) {
            perror("accept");
            exit(1);
        }
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid != 0) {
            continue;
        }
        for (;;) {
            ftph = ftph_recv(sd1);
            *lbuf = '\0';
            if (ftph->length != 0) {
                strcpy(lbuf, (char *)pld_recv(sd1, ftph->length));
            }
            for (p = typ_tbl; p->type; p++) {
                if (ftph->type == p->type) {
                    (*p->func)(sd1, lbuf);
                    break;
                }
            }
            if (p->type == 0) {
                fprintf(stderr, "unknown type\n");
            }
        }
    }
    return 0;
}

void quit_exec(int sd, char str[])
{
    data_send(sd, 0x01, 0x00, 0, NULL);
    close(sd);
    // close(sd0);
    exit(0);
}

void pwd_exec(int sd, char str[])
{
    FILE *fp;
    char *buf, *cmd = "pwd";

    if ((fp = popen(cmd, "r")) == NULL) {
        perror("popen");
        exit(1);
    }
    buf = (char *)malloc(DATASIZE);
    fgets(buf, DATASIZE, fp);
    data_send(sd, 0x10, 0x00, strlen(buf) + 1, (uint8_t *)buf);
    pclose(fp);
    free(buf);
}

void cwd_exec(int sd, char str[])
{
    if (chdir(str) < 0) {
        switch (errno) {
            case ENOENT:
                data_send(sd, 0x12, 0x00, 0, NULL);
                break;
            case EACCES:
                data_send(sd, 0x12, 0x01, 0, NULL);
                break;
            default:
                data_send(sd, 0x13, 0x05, 0, NULL);
                break;
        }
    } else {
        data_send(sd, 0x10, 0x00, 0, NULL);
    }
}

void list_exec(int sd, char str[])
{
    int pid, stat;
    FILE *fp;
    char *buf, lbuf[BUFLEN];
    char cmd[80] = "ls -l ";

    if (*str != '\0') {
        strcat(cmd, str);
        if (access(str, F_OK) != 0) {
            data_send(sd, 0x12, 0x00, 0, NULL);
            return;
        } else {
           if (access(str, R_OK) != 0) {
               data_send(sd, 0x12, 0x01, 0, NULL);
               return;
           }
        }
    }
    if ((fp = popen(cmd, "r")) == NULL) {
        data_send(sd, 0x13, 0x05, 0, NULL);
        return;
    }
    buf = (char *)malloc(DATASIZE);
    while (fgets(lbuf, sizeof lbuf, fp) != NULL) {
        lbuf[strlen(lbuf) + 2] = '\0';
        lbuf[strlen(lbuf) + 1] = '\n';
        strcat(buf, lbuf);
    }
    data_send(sd, 0x10, 0x01, 0, NULL);
    data_send(sd, 0x20, 0x00, strlen(buf) + 1, (uint8_t *)buf);
    pclose(fp);
    free(buf);
}

void retr_exec(int sd, char str[])
{
    int fd, cnt1, cnt2;
    char *buf1, *buf2;

    if ((fd = open(str, O_RDONLY)) < 0) {
        switch (errno) {
            case ENOENT:
                data_send(sd, 0x12, 0x00, 0, NULL);
                break;
            case EACCES:
                data_send(sd, 0x12, 0x01, 0, NULL);
                break;
            default:
                data_send(sd, 0x13, 0x05, 0, NULL);
                break;
        }
        return;
    } else {
        data_send(sd, 0x10, 0x01, 0, NULL);
    }
    buf1 = (char *)malloc(DATASIZE);
    cnt1 = read(fd, buf1, DATASIZE);
    for (;;) {
        if (cnt1 < 0) {
            data_send(sd, 0x12, 0x01, 0, NULL);
            perror("read");
            free(buf1);
            close(fd);
            return;
        } else if (cnt1 == DATASIZE) {
            buf2 = (char *)malloc(DATASIZE);
            cnt2 = read(fd, buf2, DATASIZE);
            if (cnt2 == 0) {
                break;
            }
            data_send(sd, 0x20, 0x00, DATASIZE, (uint8_t *)buf1);
            buf1 = buf2;
            cnt1 = cnt2;
        } else {
            break;
        }
    }
    data_send(sd, 0x20, 0x01, cnt1, (uint8_t *)buf1);
    free(buf1);
    close(fd);
}

void stor_exec(int sd, char str[])
{
    int fd;
    struct myftph *ftph;
    uint8_t *pld;

    if ((fd = open(str, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
        if (errno != EEXIST) {
            perror("open");
            exit(1);
        }
        data_send(sd, 0x12, 0x01, 0, NULL);
        return;
    } else {
        data_send(sd, 0x10, 0x02, 0, NULL);
    }
    for (;;) {
        ftph = ftph_recv(sd); 
        if (ftph->type != 0x20) {
            fprintf(stderr, "error: unexpected type 0x%x\n", ftph->type);
            return;
        } else {
            pld = (uint8_t *)malloc(ftph->length);
            pld = pld_recv(sd, ftph->length);
            if (write(fd, (char *)pld, ftph->length) < 0) {
                perror("write");
                free(pld);
                close(fd);
                exit(1);
            }
            if (ftph->code == 0x01) {
                break;
            }
        }
    }
    free(pld);
    close(fd);
}
