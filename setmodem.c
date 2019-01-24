#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#ifdef Linux
#include <sys/ioctl.h>
#endif

#define COMMAND_SIZE	256
#define VERSION		"1.0"
#define IDENT_STRING	"%s v%s Copyright 2001 \n", this, VERSION
#define HELP_STRING	"Type help or ? for help\n"

int main(int argc, char **argv) {
  char *this = *argv;
  char *delim = " ";
  char command[COMMAND_SIZE];
  char data[COMMAND_SIZE];
  int i;
  char *dummy;
  int dev = (int) NULL;
  unsigned int result;
  char prompt[COMMAND_SIZE] = "> ";
  int quit_loop;
  int signal_found;
  struct termios term;

  for (dummy = this; *dummy; dummy++)
    if (*dummy == '/')
      this = dummy + 1;

  /* Command line mode */
  if (argc > 1) {
    for (argc--, argv++; argc; argc--, argv++) {
      if (!strcmp("-g", *argv)) {
        argc--; argv++;
        if (!argc) {
          printf("No device specified\n");
          return 1;
        }
        dev = open(*argv, O_RDWR | O_NONBLOCK);
        if (dev == -1) {
          printf("Couldn't open `%s`\n", *argv);
          perror(NULL);
          return 1;
        }
        if ((tcgetattr(dev, &term)) == -1) {
          printf("Couldn't get attributes for `%s`\n", *argv);
          perror(NULL);
          return 1;
        }
	term.c_cflag |= CLOCAL;
	if ((tcsetattr(dev, TCSANOW, &term)) == -1) {
          printf("Couldn't set attributes for `%s`\n", *argv);
          perror(NULL);
          return 1;
        }
        if (ioctl(dev, TIOCMGET, &result) < 0) {
          printf("Couldn't get mode for `%s`\n", *argv);
          perror(NULL);
          return 1;
        }
        printf("Modem signals for `%s`: ", *argv);
        if (result & TIOCM_RTS) printf(" RTS "); else printf("-RTS ");
        if (result & TIOCM_CTS) printf(" CTS "); else printf("-CTS ");
        if (result & TIOCM_DSR) printf(" DSR "); else printf("-DSR ");
        if (result & TIOCM_CD) printf(" DCD "); else printf("-DCD ");
        if (result & TIOCM_DTR) printf(" DTR "); else printf("-DTR ");
        if (result & TIOCM_RI) printf(" RI  "); else printf("-RI  ");
        if (result & TIOCM_LE) printf(" LE  "); else printf("-LE  ");
        if (result & TIOCM_ST) printf(" ST  "); else printf("-ST  ");
        if (result & TIOCM_SR) printf(" SR  "); else printf("-SR  ");
        printf("\n");
        close(dev);
      } else if (!strcmp("-s", *argv)) {
        argc--; argv++;
        if (!argc) {
          printf("No device specified\n");
          return 1;
        }
        dummy = *argv;
        argc--; argv++;
        if (!argc) {
no_signals:
          printf("No modem signal(s) specified for `%s`\n", dummy);
          return 1;
        }
        dev = open(dummy, O_RDWR | O_NONBLOCK);
        if (dev == -1) {
          printf("Couldn't open `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
        if ((tcgetattr(dev, &term)) == -1) {
          printf("Couldn't get attributes for `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
	term.c_cflag |= CLOCAL;
	if ((tcsetattr(dev, TCSANOW, &term)) == -1) {
          printf("Couldn't set attributes for `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
        if (ioctl(dev, TIOCMGET, &result) < 0) {
          printf("Couldn't get mode for `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
        quit_loop = 0;
        signal_found = 0;
        for (;;) {
          if (!strncmp(*argv, "RTS", 3)) {
            result |= TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(*argv, "-RTS", 4)) {
            result &= ~TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(*argv, "rts", 3)) {
            result |= TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(*argv, "-rts", 4)) {
            result &= ~TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(*argv, "DTR", 3)) {
            result |= TIOCM_DTR; signal_found = 1;
          } else if (!strncmp(*argv, "-DTR", 4)) {
            result &= ~TIOCM_DTR; signal_found = 1;
          } else if (!strncmp(*argv, "dtr", 3)) {
            result |= TIOCM_DTR; signal_found = 1;
          } else if (!strncmp(*argv, "-dtr", 4)) {
            result &= ~TIOCM_DTR; signal_found = 1;
          } else {
            quit_loop = 1;
          }
          if (quit_loop) {
            if (!signal_found)
              goto no_signals;
            argc++; argv--;
            break;
          }
          if (!(argc - 1))
            break;
          argc--; argv++;
        }
        if (ioctl(dev, TIOCMSET, &result) < 0) {
          printf("Couldn't set mode for `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
        close(dev);
      } else if (!strcmp("-h", *argv) || !strcmp("-?", *argv)) {
        printf(IDENT_STRING);
        printf("\n");
        printf("%s [-g <dev>] [-s <dev> <signal> [signal ...]] [-g ...] [-s ...]\n", this);

        printf("\n");
        printf("-g <dev>\t\tGet device <dev>'s modem signals\n");
        printf("-s <dev> <mode>\t\tSet device <dev>'s modem signals: [-]RTS [-DTR]\n");
        printf("\n");
        printf("Multiple gets/sets are allowed in any order on the command "
                                                                "line\n");
        printf("%s will exit after the first error\n", this);
        printf("\n");
      } else {
        printf("%s: Error: Unrecognized option `%s`\n", this, *argv);
        return 1;
      }
    }

    return 0;
  }

  /* Interactive mode */
  printf(IDENT_STRING);
  printf(HELP_STRING);

  do {
    printf("%s", prompt);
    for (i = 0; i < COMMAND_SIZE - 1; i++) {
      command[i] = getc(stdin);
      if (command[i] == '\n')
        break;
      if (command[i] == '\t')
        command[i] = ' ';
      if (((i == 0) && (command[i] == ' ')) ||
          ((i > 0) && (command[i] == ' ') && (command[i - 1] == ' ')))
        i--;
    }
    command[i] = (char) NULL;
    strcpy(data, command);
    if ((data[0] >= 'A') && (data[0] <= 'Z'))
      data[0] += ' ';
    strtok(data, delim);
    switch (data[0]) {
      case 's':
        if (!dev) {
          printf("No device currently open\n");
          break;
        }
        dummy = strtok(NULL, delim);
        if (!dummy) {
          printf("You need to specify one or more signals: [-]RTS [-]DTR\n");
          break;
        }
        if (ioctl(dev, TIOCMGET, &result) < 0) {
          printf("Couldn't get mode\n");
          perror(NULL);
          break;
        }
        signal_found = 0;
        for (;;) {
          if (!strncmp(dummy, "RTS", 3)) {
            result |= TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(dummy, "-RTS", 4)) {
            result &= ~TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(dummy, "rts", 3)) {
            result |= TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(dummy, "-rts", 4)) {
            result &= ~TIOCM_RTS; signal_found = 1;
          } else if (!strncmp(dummy, "DTR", 3)) {
            result |= TIOCM_DTR; signal_found = 1;
          } else if (!strncmp(dummy, "-DTR", 4)) {
            result &= ~TIOCM_DTR; signal_found = 1;
          } else if (!strncmp(dummy, "dtr", 3)) {
            result |= TIOCM_DTR; signal_found = 1;
          } else if (!strncmp(dummy, "-dtr", 4)) {
            result &= ~TIOCM_DTR; signal_found = 1;
          } else {
            printf("Bad signal(s) `%s`\n", command);
            signal_found = 0;
            break;
          }
          dummy = strtok(NULL, delim);
          if (!dummy)
            break;
        }
        if (!signal_found)
          break;
        if (ioctl(dev, TIOCMSET, &result) < 0) {
          printf("Couldn't set mode\n");
          perror(NULL);
        }
        break;
      case 'g':
        if (!dev) {
          printf("No device currently open\n");
          break;
        }
        if (ioctl(dev, TIOCMGET, &result) < 0) {
          printf("Couldn't get mode\n");
          perror(NULL);
          break;
        }
        printf("Modem signals: ");
        if (result & TIOCM_RTS) printf(" RTS "); else printf("-RTS ");
        if (result & TIOCM_CTS) printf(" CTS "); else printf("-CTS ");
        if (result & TIOCM_DSR) printf(" DSR "); else printf("-DSR ");
        if (result & TIOCM_CD) printf(" DCD "); else printf("-DCD ");
        if (result & TIOCM_DTR) printf(" DTR "); else printf("-DTR ");
        if (result & TIOCM_RI) printf(" RI  "); else printf("-RI  ");
        if (result & TIOCM_LE) printf(" LE  "); else printf("-LE  ");
        if (result & TIOCM_ST) printf(" ST  "); else printf("-ST  ");
        if (result & TIOCM_SR) printf(" SR  "); else printf("-SR  ");
        printf("\n");
        break;
      case 'o':
        dummy = strtok(NULL, delim);
        if (!dummy) {
          printf("You need to specify a device to open\n");
          break;
        }
        if (dev)
          close(dev);
        dev = open(dummy, O_RDWR | O_NONBLOCK);
        if (dev == -1) {
          printf("Couldn't open `%s`\n", dummy);
          perror(NULL);
          break;
        }
        if ((tcgetattr(dev, &term)) == -1) {
          printf("Couldn't get attributes for `%s`\n", dummy);
          perror(NULL);
          break;
        }
	term.c_cflag |= CLOCAL;
	if ((tcsetattr(dev, TCSANOW, &term)) == -1) {
          printf("Couldn't set attributes for `%s`\n", dummy);
          perror(NULL);
          break;
        }
        sprintf(prompt, "%s> ", dummy);
        break;
      case 'c':
        if (!dev) {
          printf("No device currently open\n");
          break;
        }
        close(dev);
        sprintf(prompt, "> ");
        break;
      case 'h':
      case '?':
        printf(IDENT_STRING);
        printf("\n");
        printf("close\t\t\tClose the current device\n");
        printf("get\t\t\tGet the current device's modem signals\n");
        printf("help\t\t\tThis help screen\n");
        printf("open <dev>\t\tOpen device <dev>\n");
        printf("quit\t\t\tExit %s\n", this);
        printf("set <signal>\t\tSet device's modem signals: [-]RTS [-]DTR\n");
        printf("\n");
        printf("Commands can be abbreviated to the first letter\n");
        printf("\n");
        printf("To get command line help, run %s -h\n", this);
        break;
      case 'q':
      case '\0':
        break;
      default:
        printf("Invalid command `%s`\n", command);
        printf(HELP_STRING);
        break;
    }
  } while (data[0] != 'q');

  return 0;
}
