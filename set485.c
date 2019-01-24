#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(Linux)

#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/serial.h>

#define FULLDUPLEX		TIOCSER485FULLDUPLEX
#define HALFDUPLEX		TIOCSER485HALFDUPLEX
#define SLAVEMULTI		TIOCSER485SLAVEMULTIPLEX
#define SET485			TIOCSER485SET
#define GET485			TIOCSER485GET

#elif defined(SCO_SV)

#include <sys/sio.h>

#define FULLDUPLEX		AIOC485FULLDUPLEX
#define HALFDUPLEX		AIOC485HALFDUPLEX
#define SLAVEMULTI		AIOC485SLAVEMULTI
#define SET485			result
#define GET485			AIOC485GETMODE

#elif defined(SunOS)

#include <sys/termios.h>

#define FULLDUPLEX		TIOC485FULLDUPLEX
#define HALFDUPLEX		TIOC485HALFDUPLEX
#define SLAVEMULTI		TIOC485SLAVEMULTIPLEX
#define SET485			TIOC485SETMODE
#define GET485			TIOC485GETMODE

#endif

#define COMMAND_SIZE	256
#define VERSION		"1.0"
#define IDENT_STRING	"%s v%s Copyright 1999 \n", this, VERSION
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
        /* dev = open(*argv, O_RDWR); */
        dev = open(*argv, O_RDWR | O_NONBLOCK);
        if (dev == -1) {
          printf("Couldn't open '%s'\n", *argv);
          perror(NULL);
          return 1;
        }
        /* New open code */
        i = fcntl(dev, F_GETFL, NULL);
        if (i == -1) {
          printf("Couldn't get file flags for `%s`\n", *argv);
          perror(NULL);
          return 1;
        }
        i = fcntl(dev, F_SETFL, i & ~O_NONBLOCK);
        if (i == -1) {
          printf("Couldn't set file flags for `%s`\n", *argv);
          perror(NULL);
          return 1;
        }
        if (ioctl(dev, GET485, &result) < 0) {
          printf("Couldn't get mode for '%s'\n", *argv);
          perror(NULL);
          return 1;
        }
        switch (result) {
          case FULLDUPLEX:
            printf("Full duplex for '%s'\n", *argv);
            break;
          case HALFDUPLEX:
            printf("Half duplex for '%s'\n", *argv);
            break;
          case SLAVEMULTI:
            printf("Slave multiplex for '%s'\n", *argv);
            break;
          default:
            printf("Unknown mode '%d' for '%s'\n", result, *argv);
            break;
        }
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
          printf("No mode specified for '%s'\n", dummy);
          return 1;
        }
        /* dev = open(dummy, O_RDWR); */
        dev = open(dummy, O_RDWR | O_NONBLOCK);
        if (dev == -1) {
          printf("Couldn't open '%s'\n", dummy);
          perror(NULL);
          return 1;
        }
        /* New open code */
        i = fcntl(dev, F_GETFL, NULL);
        if (i == -1) {
          printf("Couldn't get file flags for `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
        i = fcntl(dev, F_SETFL, i & ~O_NONBLOCK);
        if (i == -1) {
          printf("Couldn't set file flags for `%s`\n", dummy);
          perror(NULL);
          return 1;
        }
        if ((*argv[0] == 'f') || (*argv[0] == 'F'))
          result = FULLDUPLEX;
        else if ((*argv[0] == 'h') || (*argv[0] == 'H'))
          result = HALFDUPLEX;
        else if ((*argv[0] == 's') || (*argv[0] == 'S'))
          result = SLAVEMULTI;
        else {
          printf("Invalid mode '%s' for '%s'\n", *argv, dummy);
          return 1;
        }
        if (ioctl(dev, SET485, &result) < 0) {
          printf("Couldn't set mode for '%s'\n", dummy);
          perror(NULL);
          return 1;
        }
        close(dev);
      } else if (!strcmp("-h", *argv) || !strcmp("-?", *argv)) {
        printf(IDENT_STRING);
        printf("\n");
        printf("%s [-g <dev>] [-s <dev> <mode>] [-g ...] [-s ...]\n", this);
        printf("\n");
        printf("-g <dev>\t\tGet device <dev>'s mode\n");
        printf("-s <dev> <mode>\t\tSet device <dev> to 485 mode full, half "
                                                                "or slave\n");
        printf("-h\t\t\tThis help screen\n");
        printf("\n");
        printf("Modes can be abbreviated to the first letter\n");
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
          printf("You need to specify a mode: full, half or slave\n");
          break;
        }
        if ((dummy[0] >= 'A') && (dummy[0] <= 'Z'))
          dummy[0] += ' ';
        if (dummy[0] == 'f')
          result = FULLDUPLEX;
        else if (dummy[0] == 'h')
          result = HALFDUPLEX;
        else if (dummy[0] == 's')
          result = SLAVEMULTI;
        else {
          printf("Invalid mode '%s'; try full, half or slave\n", command);
          break;
        }
        if (ioctl(dev, SET485, &result) < 0) {
          printf("Couldn't set mode\n");
          perror(NULL);
        }
        break;
      case 'g':
        if (!dev) {
          printf("No device currently open\n");
          break;
        }
        if (ioctl(dev, GET485, &result) < 0) {
          printf("Couldn't get mode\n");
          perror(NULL);
          break;
        }
        switch (result) {
          case FULLDUPLEX:
            printf("Full duplex\n");
            break;
          case HALFDUPLEX:
            printf("Half duplex\n");
            break;
          case SLAVEMULTI:
            printf("Slave multiplex\n");
            break;
          default:
            printf("Unknown mode '%d %x'\n", result, result);
            break;
        }
        break;
      case 'o':
        dummy = strtok(NULL, delim);
        if (!dummy) {
          printf("You need to specify a device to open\n");
          break;
        }
        if (dev)
          close(dev);
        /* dev = open(dummy, O_RDWR); */
        dev = open(dummy, O_RDWR | O_NONBLOCK);
        if (dev == -1) {
          printf("Couldn't open '%s'\n", dummy);
          perror(NULL);
          break;
        }
        /* New open code */
        i = fcntl(dev, F_GETFL, NULL);
        if (i == -1) {
          close(dev);
          printf("Couldn't get file flags for `%s`\n", dummy);
          perror(NULL);
          break;
        }
        i = fcntl(dev, F_SETFL, i & ~O_NONBLOCK);
        if (i == -1) {
          close(dev);
          printf("Couldn't set file flags for `%s`\n", dummy);
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
        printf("get\t\t\tGet the current device's mode\n");
        printf("help\t\t\tThis help screen\n");
        printf("open <dev>\t\tOpen device <dev>\n");
        printf("quit\t\t\tExit %s\n", this);
        printf("set <mode>\t\tSet device to 485 mode full, half or slave\n");
        printf("\n");
        printf("Commands and modes can be abbreviated to the first letter\n");
        printf("\n");
        printf("To get command line help, run %s -h\n", this);
        break;
      case 'q':
      case '\0':
        break;
      default:
        printf("Invalid command '%s'\n", command);
        printf(HELP_STRING);
        break;
    }
  } while (data[0] != 'q');

  return 0;
}
