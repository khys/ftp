// myFTP client

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
#include <ctype.h>

#include "myftp.h"

struct command_table {
    char *cmd;
    void (*func)(int, int, char *[]);
} cmd_tbl[] = {
    {"quit", quit_proc},
    {"pwd", pwd_proc},
    {"cd", cd_proc},
    {"dir", dir_proc},
    {"lpwd", lpwd_proc},
    {"lcd", lcd_proc},
    {"ldir", ldir_proc},
    {"get", get_proc},
    {"put", put_proc},
    {"help", help_proc},
    {NULL, NULL}
};

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res;
    char buf[80];
    int sd, err;
    struct command_table *p;
    int ac;
    char *av[NARGS];

    if (argc != 3) {
        fprintf(stderr,
                "Usage: ./myftpc <server hostname> <port number>\n");
        exit(1);
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    if ((err = getaddrinfo(argv[1], argv[2], &hints, &res)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(1);
    }
    if ((sd = socket(res->ai_family, res->ai_socktype,
                     res->ai_protocol)) < 0) {
        perror("socket");
        exit(1);
    }
    if (connect(sd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(1);
    }
    freeaddrinfo(res);

    for (;;) {
        fprintf(stderr, "myFTP%% ");
        fgets(buf, sizeof buf, stdin);
        buf[strlen(buf) - 1] = '\0';
        if (*buf == '\0') {
            continue;
        }
        getargs(&ac, av, buf);
        for (p = cmd_tbl; p->cmd; p++) {
            if (!strcmp(av[0], p->cmd)) {
                (*p->func)(sd, ac, av);
                break;
            }
        }
        if (p->cmd == NULL) {
            fprintf(stderr, "unknown command: %s\n", av[0]);
        }
    }
    return 0;
}

void getargs(int *ac, char *av[], char *p)
{
    *ac = 0;

    for (;;) {
        while (isblank(*p)) {
            p++;
        }
        if (*p == '\0') {
            return;
        }
        av[(*ac)++] = p;
        while (*p && !isblank(*p)) {
            p++;
        }
        if (*p == '\0') {
            return;
        }
        *p++ = '\0';
    }
}

void quit_proc(int sd, int ac, char *av[])
{
    struct myftph *ftph;

    data_send(sd, 0x01, 0x00, 0, NULL);
    ftph = ftph_recv(sd);
    close(sd);
    exit(0);
}

void pwd_proc(int sd, int ac, char *av[])
{
    struct myftph *ftph;
    uint8_t *pld;

    data_send(sd, 0x02, 0x00, 0, NULL);
    ftph = ftph_recv(sd);
    if (ftph->length == 0) {
        fprintf(stderr, "error\n");
        return;
    }
    pld = (uint8_t *)malloc(ftph->length);
    pld = pld_recv(sd, ftph->length);
    if (ftph->type != 0x10) {
        fprintf(stderr, "error\n");
    }
    printf("%s", (char *)pld);
}

void cd_proc(int sd, int ac, char *av[])
{
    struct myftph *ftph;

    if (ac != 2) {
        fprintf(stderr, "Syntax: cd <directory>\n");
        return;
    }
    data_send(sd, 0x03, 0x00, strlen(av[1]) + 1, (uint8_t *)av[1]);
    ftph = ftph_recv(sd);
    switch (ftph->type) {
        case 0x10:
            break;
        case 0x11:
            break;
        case 0x12:
            if (ftph->code == 0x00) {
                fprintf(stderr, "cd: %s: No such file or directory\n",
                        av[1]);
            } else if (ftph->code == 0x01) {
                fprintf(stderr, "%s: Permission denied\n", av[1]);
            }
            break;
        case 0x13:
                fprintf(stderr, "Undefined error\n");
            break;
    }
}

void dir_proc(int sd, int ac, char *av[])
{
    struct myftph *ftph;
    uint8_t *pld;

    if (ac > 3) {
        fprintf(stderr, "Syntax: dir [file or directory]\n");
        return;
    } else if (ac == 1) {
        data_send(sd, 0x04, 0x00, 0, NULL);
    } else {
        data_send(sd, 0x04, 0x00, strlen(av[1]) + 1, (uint8_t *)av[1]);
    }
    ftph = ftph_recv(sd);
    switch (ftph->type) {
        case 0x10:
            break;
        case 0x11:
            break;
        case 0x12:
            if (ftph->code == 0x00) {
                fprintf(stderr, "cd: %s: No such file or directory\n",
                        av[1]);
            } else if (ftph->code == 0x01) {
                fprintf(stderr, "%s: Permission denied\n", av[1]);
            }
            return;
        case 0x13:
            fprintf(stderr, "Undefined error\n");
            return;
    }
    ftph = ftph_recv(sd);
    if (ftph->type != 0x20) {
        fprintf(stderr, "Undefined error\n");
        return;
    }
    pld = (uint8_t *)malloc(ftph->length);
    pld = pld_recv(sd, ftph->length);
    printf("%s", (char *)pld);
}

void lpwd_proc(int sd, int ac, char *av[])
{
    int pid, stat;
    char *ex[] = {"pwd", NULL};

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        if (execvp(ex[0], ex) < 0) {
            perror("execvp");
            exit(1);
        }
    }
    if (wait(&stat) < 0) {
        perror("wait");
        exit(1);
    }
}

void lcd_proc(int sd, int ac, char *av[])
{
    if (ac != 2) {
        fprintf(stderr, "Syntax: lcd <directory>\n");
    } else if (chdir(av[1]) < 0) {
        fprintf(stderr, "chdir error\n");
        exit(1);
    }
}

void ldir_proc(int sd, int ac, char *av[])
{
    int pid, stat;
    char *ex1[] = {"ls", "-l", NULL};
    char *ex2[] = {"ls", "-l", av[1], NULL};

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        if (ac == 1) {
            if (execvp(ex1[0], ex1) < 0) {
                perror("execvp");
                exit(1);
            }
        } else {
            if (execvp(ex2[0], ex2) < 0) {
                perror("execvp");
                exit(1);
            }
        }
    }
    if (wait(&stat) < 0) {
        perror("wait");
        exit(1);
    }
}

void get_proc(int sd, int ac, char *av[])
{
    int fd;
    char file[80], lbuf[10];
    struct myftph *ftph;
    uint8_t *pld;

    if (ac > 3 || ac < 2) {
        fprintf(stderr, "Syntax: get <filename> [filename]\n");
        return;
    } else if (ac == 2) {
        strcpy(file, av[1]);
    } else {
        strcpy(file, av[2]);
    }
    if ((fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
        if (errno != EEXIST) {
            perror("open");
            exit(1);
        }
        fprintf(stderr, "overwrite ok (yes/no): ");
        if (fgets(lbuf, sizeof lbuf, stdin) == NULL) {
            if (ferror(stdin)) {
                perror("fgets");
                exit(1);
            }
            if (feof(stdin)) {
                fprintf(stderr, "stdin EOF\n");
                exit(1);
            }
        }
        if (*lbuf != 'y') {
            return;
        }
        if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
            perror("open");
            exit(1);
        }
    }

    data_send(sd, 0x05, 0x00, strlen(av[1]) + 1, (uint8_t *)av[1]);
    ftph = ftph_recv(sd);
    if (ftph->type != 0x10) {
        fprintf(stderr, "error: 0x10\n");
        return;
    }
    if (ftph->code != 0x01) {
        fprintf(stderr, "no data\n");
        return;
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
            free(pld);
            if (ftph->code == 0x01) {
                break;
            }
        }
    }
    close(fd);
}

void put_proc(int sd, int ac, char *av[])
{
    int fd, cnt;
    char *filename, *buf;
    struct myftph *ftph;

    if (ac > 3 || ac < 2) {
        fprintf(stderr, "Syntax: put <filename> [filename]\n");
        return;
    } else if (ac == 2) {
        filename = av[1];
    } else {
        filename = av[2];
    }
    data_send(sd, 0x06, 0x00, strlen(filename) + 1, (uint8_t *)filename);
    ftph = ftph_recv(sd);
    if (ftph->type != 0x10) {
        fprintf(stderr, "error\n");
        return;
    }
    if (ftph->code != 0x02) {
        fprintf(stderr, "no data\n");
        return;
    }
    if ((fd = open(av[1], O_RDONLY)) < 0) {
        perror("open");
        return;
    }
    buf = (char *)malloc(DATASIZE);
    while ((cnt = read(fd, buf, DATASIZE))) {
        if (cnt < 0) {
            data_send(sd, 0x12, 0x01, 0, NULL);
            perror("read");
            close(fd);
            return;
        } else if (cnt == DATASIZE) {
            data_send(sd, 0x20, 0x00, cnt, (uint8_t *)buf);
        } else {
            break;
        }
    }
    data_send(sd, 0x20, 0x01, cnt, (uint8_t *)buf);
    free(buf);

    close(fd);
}

void help_proc(int sd, int ac, char *av[]){}
