#ifndef TTY_H
#define TTY_H

#define TTY_LINEMAX	1024
#define TTY_PROMPTMAX	64

struct pollfd;

int tty_init(void (*)(void *, char *), void *);
void tty_gets(void);
void tty_puts(char *);
void tty_printf(char *, ...);
void tty_done(void);
void tty_setprompt(char *);
void tty_setline(char *);
int tty_pollfd(struct pollfd *);
int tty_revents(struct pollfd *);
void tty_winch(void);
void tty_reset(void);
void tty_write(void *, size_t);

#endif
