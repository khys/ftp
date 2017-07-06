// myFTP

// #define DATASIZE (100 * 1024 * 1024)
#define DATASIZE (1 * 1 * 1024)
#define NARGS    256
#define BUFLEN   1024

struct myftph {
    uint8_t  type;
    uint8_t  code;
    uint16_t length;
};

void getargs(int *, char *[], char *);

void quit_proc(int, int, char *[]);
void pwd_proc(int, int, char *[]);
void cd_proc(int, int, char *[]);
void dir_proc(int, int, char *[]);
void lpwd_proc(int, int, char *[]);
void lcd_proc(int, int, char *[]);
void ldir_proc(int, int, char *[]);
void get_proc(int, int, char *[]);
void put_proc(int, int, char *[]);
void help_proc(int, int, char *[]);

void quit_exec(int, char []);
void pwd_exec(int, char []);
void cwd_exec(int, char []);
void list_exec(int, char []);
void retr_exec(int, char []);
void stor_exec(int, char []);

void data_send(int, int, int, int, uint8_t *);
struct myftph *ftph_recv(int);
uint8_t *pld_recv(int, int);
