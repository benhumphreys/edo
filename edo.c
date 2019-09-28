#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

// Editor modes
enum modes {
    NORMAL,
    INSERT
};

// ASCII key
#define KEY_ESC 0x1b

enum modes mode;

void die(const char *s) {
    perror(s);
    exit(1);
}

void restore_tmode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    };
}

void raw_tmode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(restore_tmode);

    struct termios raw = orig_termios;

    // ~ISIG disables signals send due to CTRL-C and CTRL-Z
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    // Disables CTRL-S and CTRL-Q (in-band software flow control)
    raw.c_iflag &= ~(IXON | ICRNL);

    raw.c_oflag &= ~(OPOST);

    raw.c_cflag |= CS8;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

void printkey(char c) {
    if (iscntrl(c)) {
        printf("%d\r\n", c);
    } else {
        printf("%d ('%c')\r\n", c, c);
    }
}

char read_keypress() {
    int n;
    char c;
    while ((n = read(STDIN_FILENO, &c, 1)) != 1) {
        if (n == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    return c;
}

void process_keypress() {
    char c = read_keypress();
    printkey(c);

    switch (c) {
        case 'q':
            exit(0);
            break;
        case KEY_ESC:
            if (mode == INSERT) {
                mode = NORMAL;
            }
            break;
    }
}

int main(int argc, const char* argv[]) {
    raw_tmode();

    mode = NORMAL;

    while (1) {
        process_keypress();
    }

    return 0;
}
