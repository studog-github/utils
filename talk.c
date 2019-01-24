#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define BUF_DEFAULT	"256"
#define DEV_DEFAULT	"/dev/tty1_1a"
#define BAUD_DEFAULT	"9600"
#define CLOCK_DEFAULT	"0"

#define FLAG_FILE	"sink.flag"

#if defined(Linux)

#if defined(__uClinux__)
#define MAXSIG		NSIG
#else
#define MAXSIG		_NSIG
#endif
#define RTSFLOW		CRTSCTS
#define CTSFLOW		CRTSCTS

#endif

#if defined(SunOS)

#define RTSFLOW		CRTSXOFF
#define CTSFLOW		CRTSCTS

#endif

char *version = "2.60";
char *revdate = "Jul 14 2004";

/*
 * Change Log
 *
 * 2.60 - 2004 07 14 - stu
 *
 * - header file reorg
 */

#define CS9	004000000000

void dump_buf(FILE *log, char *temp, int size, int *stat, int idx) {
	int i;
	int j;
	int k;

	for (i = 0, j = 1, k = 0; i < size; i++, j++) {
		if (j == stat[k]) {
			j = 0;
			k++;
			fprintf(log, "%c", (temp[i] - 32));
		} else
			fprintf(log, "%c", temp[i]);
	}
}

char *etime(time_t elapsed) {
	static char string[13];

	sprintf(string, "%3d %02d:%02d:%02d",
		(int) (elapsed / (60 * 60 * 24)),
		(int) ((elapsed / (60 * 60)) % 24),
		(int) ((elapsed / 60) % 60),
		(int) (elapsed % 60));

	return string;
}

void sig_handler(int sig) {
	printf("\n");
printf("sig_handler exit on %d\n", sig);
	exit(0);
}

int main(int argc, char **argv) {
	char *this;
	char *dev_name = DEV_DEFAULT;
	int dev;
	int buf_size = atoi(BUF_DEFAULT);
	int buf_size9 = 0;
	char *template;
	char *template9 = NULL;
	int i;
	int got;
	time_t start = (time_t) NULL;
	time_t elapsed = 0;
	time_t old_elapsed;
	unsigned int txed = 0;
	unsigned int old_txed = 0;
	unsigned int roll_count = 0;
	struct sigaction action;
	int baud = atoi(BAUD_DEFAULT);
	int pbaud;
	struct termios term;
	int tx_stat[256];
	int tx_idx;
	unsigned long pkt_count = 0;
	char dummy_buf[11];
	int multiclock = atoi(CLOCK_DEFAULT);
	int bin_data = 0;
	int garble_data = 0;
	int no_flow = 0;
	int fast_updates = 0;
	int randfile = 0;
	int randloop = 0;
	int randnum;
	char randchar = (char) NULL;
	int randsub = 0;
	int bit9_mode = 0;
/* FILE *log; */

	action.sa_handler = sig_handler;
	for (i = 1; i < MAXSIG; i++)
		sigaction(i, &action, NULL);

	this = *argv;
	for (argc--, argv++; argc; argc--, argv++) {
		if (!strcmp("-d", *argv)) {
			argc--; argv++;
			if (!argc) break;
			dev_name = *argv;
		} else if (!strcmp("-b", *argv)) {
			argc--; argv++;
			if (!argc) break;
			baud = atoi(*argv);
		} else if (!strcmp("-s", *argv)) {
			argc--; argv++;
			if (!argc) break;
			buf_size = atoi(*argv);
		} else if (!strcmp("-x", *argv)) {
			argc--; argv++;
			if (!argc) break;
			multiclock = atoi(*argv);
		} else if (!strcmp("-B", *argv)) {
			bin_data = 1;
		} else if (!strcmp("-g", *argv)) {
			garble_data = 1;
		} else if (!strcmp("-f", *argv)) {
			no_flow = 1;
		} else if (!strcmp("-u", *argv)) {
			fast_updates = 1;
		} else if (!strcmp("-9", *argv)) {
			bit9_mode = 1;
			garble_data = 0;
		} else if (!strcmp("-v", *argv)) {
			printf("%s v%s %s\n", this, version, revdate);
			return 0;
		} else if (!strcmp("-h", *argv) ||
				!strcmp("-?", *argv)) {
			printf("\n");
			printf("%s [-d dev] [-s size] [-b baud] [-x clock] [-Bgfuv9]\n", this);
			printf("\n");
			printf("-b baud\t\tbaud rate [" BAUD_DEFAULT "]\n");
			printf("-d dev\t\tdevice to attach to [" DEV_DEFAULT "]\n");
			printf("-s size\t\tpacket size [" BUF_DEFAULT "]\n");
			printf("-x clock\toverclock factor [" CLOCK_DEFAULT "]\n");
			printf("-B\t\tuse binary (8 bit) data instead of 7 bit\n");
			printf("-g\t\trandomly add a one byte error to some packets\n");
			printf("-f\t\tdon't use flow control\n");
			printf("-u\t\tdo fast updates (sucks CPU cycles though)\n");
			printf("-9\t\tuse 9 bit mode\n");
			return 0;
		} else {
			printf("%s: Error: Unrecognized option `%s`\n", this,
									*argv);
			return 1;
		}
	}

	if (garble_data) {
		randfile = open("/dev/urandom", O_RDONLY);
		if (randfile == -1) {
			printf("Couldn't open /dev/urandom\n");
			perror(this);
			return 1;
		}
	}

	if (buf_size <= 0)
		buf_size = atoi(BUF_DEFAULT);
	if (bit9_mode)
		buf_size9 = buf_size * 2;
	switch (baud) {
		case 50: baud = B50; pbaud=50;  break;
		case 75: baud = B75; pbaud=75;  break;
		case 110: baud = B110; pbaud=110;  break;
		case 134: baud = B134; pbaud=134;  break;
		case 150: baud = B150; pbaud=150;  break;
		case 200: baud = B200; pbaud=200;  break;
		case 300: baud = B300; pbaud=300;  break;
		case 600: baud = B600; pbaud=600;  break;
		case 1200: baud = B1200; pbaud=1200;  break;
		case 1800: baud = B1800; pbaud=1800;  break;
		case 2400: baud = B2400; pbaud=2400;  break;
		case 4800: baud = B4800; pbaud=4800;  break;
		default:
		case 9600: baud = B9600; pbaud=9600;  break;
		case 19200: baud = B19200; pbaud=19200;  break;
		case 38400: baud = B38400; pbaud=38400;  break;
		case 57600: baud = B57600; pbaud=57600;  break;
#if defined(SCO_SV) || defined(SunOS)
		case 76800: baud = B76800; pbaud=76800;  break;
#endif
		case 115200: baud = B115200; pbaud=115200;  break;
#if defined(SunOS)
		case 153600: baud = B153600; pbaud=153600;  break;
#endif
		case 230400: baud = B230400; pbaud=230400;  break;
#if defined(SunOS)
		case 307200: baud = B307200; pbaud=307200;  break;
#endif
		case 460800: baud = B460800; pbaud=460800;  break;
		case 921600: baud = B921600; pbaud=921600;  break;
#if defined(SCO_SV) || defined(SunOS)
		case 1382400: baud = B1382400; pbaud=1382400;  break;
#endif
	}

	template = (char *) malloc(sizeof(char) * buf_size);
	if (!template) {
		printf("%s: Error: Couldn't malloc template buffer\n", this);
		perror(this);
		return 1;
	}
	if (bit9_mode) {
		template9 = (char *) malloc(sizeof(char) * buf_size9);
		if (!template9) {
			printf("%s: Error: Couldn't malloc template9 buffer\n",
									this);
			perror(this);
			return 1;
		}
	}

	strncpy(template, "1234567890", (10 < buf_size ? 10 : buf_size));
	for (i = 10; i < buf_size; i++)
		template[i] = (char) ((i % 80) % 26 + (bin_data ? 128 : 97));
	if (bit9_mode) {
		strncpy(template9, "01020304050607080900",
					(20 < buf_size9 ? 20 : buf_size9));
		/* Yes this is the same for-loop as the regular buffer */
		for (i = 10; i < buf_size; i++) {
			template9[2 * i + 1] =
				(char) ((i % 80) % 26 + (bin_data ? 128 : 97));
			template9[2 * i] = (char) (template9[2 * i + 1] & 0x01);
		}
	}

	dev = open(dev_name, O_WRONLY | O_NONBLOCK);
	if (dev < 0) {
		printf("%s: Error: Couldn't open `%s` for writing\n", this,
								dev_name);
		perror(this);
		return 1;
	}
	i = fcntl(dev, F_GETFL, NULL);
	if (i == -1) {
		printf("%s: Error: Couldn't get file flags for `%s`\n", this,
								dev_name);
		perror(this);
		return 1;
	}
	i = fcntl(dev, F_SETFL, i & ~O_NONBLOCK);
	if (i == -1) {
		printf("%s: Error: Couldn't set file flags for `%s`\n", this,
								dev_name);
		perror(this);
		return 1;
	}
	i = tcgetattr(dev, &term);
	if (i == -1) {
		printf("%s: Error: Couldn't get attributes for `%s`\n", this,
								dev_name);
		perror(this);
		return 1;
	}
#if defined(SCO_SV)
	if (baud == B1382400)
		term.c_cflag = 0x18070cb7;
	else {
#endif
		i = cfsetispeed(&term, baud);
		if (i == -1) {
			printf("%s: Error: Couldn't set baud rate for `%s`\n",
								this, dev_name);
			perror(this);
			return 1;
		}
		i = cfsetospeed(&term, baud);
		if (i == -1) {
			printf("%s: Error: Couldn't set baud rate for `%s`\n",
								this, dev_name);
			perror(this);
			return 1;
		}
#if defined(SCO_SV)
	}
#endif
	term.c_cflag |= HUPCL;
	if (!no_flow)
		term.c_cflag |= RTSFLOW | CTSFLOW;
	if (bit9_mode)
		term.c_cflag = (term.c_cflag & ~CSIZE) | CS9;
	term.c_iflag &= ~(IXON | IXOFF | IXANY);
	term.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHOKE|IEXTEN);
	term.c_oflag &= ~(OPOST);
        term.c_cc[VMIN] = 1;
	i = tcsetattr(dev, TCSANOW, &term);
	if (i == -1) {
		printf("%s: Error: Couldn't set attributes for `%s`\n", this,
								dev_name);
		perror(this);
		return 1;
	}
	i = tcgetattr(dev, &term);
	if (i == -1) {
		printf("%s: Error: Couldn't get attributes for `%s`\n", this,
								dev_name);
		perror(this);
		return 1;
	}
	if (cfgetispeed(&term) != baud) {
		printf("%s: Error: Couldn't set ibaud rate for `%s`\n", this,
								dev_name);
		return 1;
	}
	if (cfgetospeed(&term) != baud) {
		printf("%s: Error: Couldn't set obaud rate for `%s`\n", this,
								dev_name);
		return 1;
	}

	if (garble_data) {
		/*
		* Subtle difference between this primer randloop set and the
		* one in the loop; this one can be 0 to allow the first packet
		* to be garbled. The main loop one can't be zero, because then
		* it'll decrement to -1 after the write loop and never garble
		* again.
		* Another way to look at it: this loop specifies the number of
		* packets that can be written before the first garble happens,
		* which can be 0 - 19. The main loop specifies the nth packet
		* to be garbled, which is 1 - 20. It can't be 0, since we're
		* already garbling the current packet.
		*/
		read(randfile, &randloop, sizeof(randloop));
		randloop %= 20;
		if (randloop < 0)
			randloop += 20;
	}
	tcflush(dev, TCOFLUSH);
	unlink(FLAG_FILE);
	sleep(1); /* wait for sink to open on Solaris */
/* log = fopen("talk.log", "w"); */
	while (1) {
		tx_idx = 0;
		if (fopen(FLAG_FILE, "r"))
{
/* fprintf(log, "flag file found A, exiting\n"); */
			break;
}
#if defined(SCO_SV)
		sprintf(dummy_buf, "%010d", pkt_count);
#elif defined(Linux) || defined(SunOS)
		sprintf(dummy_buf, "%010ld", pkt_count);
#endif
		/*
		 * Needs to be before the strncpy, since the garbled char
		 * might have been in the serial number.
		 */
		if (garble_data && randsub) {
			template[randnum] = randchar;
			randsub = 0;
		}
		if (bit9_mode) {
			for (i = 0; i < 10; i++) {
				template9[2 * i + 1] = dummy_buf[i];
				template9[2 * i] = template9[2 * i + 1] & 0x01;
			}
		} else
			strncpy(template, dummy_buf,
					(10 < buf_size ? 10 : buf_size));
		if (garble_data && randloop == 0) {
			read(randfile, &randloop, sizeof(randloop));
			randloop %= 20;
			if (randloop < 0)
				randloop += 20;
			randloop++;
			read(randfile, &randnum, sizeof(randnum));
			randnum %= buf_size;
			if (randnum < 0)
				randnum += buf_size;
			randchar = template[randnum];
			template[randnum] = '^';
			randsub = 1;
		}
		for (i = 0; i < (bit9_mode ? buf_size9 : buf_size); i += got) {
			if (bit9_mode)
				got = write(dev, (template9 + i),
								buf_size9 - i);
			else
				got = write(dev, (template + i), buf_size - i);
/* fprintf(log, "Packet %010ld loop %2d wrote %d\n", pkt_count, i, got); */
			tx_stat[tx_idx++] = (bit9_mode ? got / 2 : got);
			if (!start) {
				start = time(NULL);
				printf("%s started %s", this, ctime(&start));
				printf("on %s size %d ", dev_name, buf_size);
				printf("speed %d", pbaud);
				if (multiclock)
					printf(" (x %d = %d)", multiclock,
							pbaud * multiclock);
			}
			if (got == -1) {
				printf("\n%s: Failure: Couldn't write "
                                       "buffer (%d)\n", this, errno);
				perror(this);
                                /* while (1) sleep(60); */
/* fprintf(log, "Bad write, exiting\n"); */
				exit(1);
				/* got = 0; */
			}
		if (fopen(FLAG_FILE, "r"))
{
/* fprintf(log, "flag file found B, exiting\n"); */
			break;
}
		}
		if (garble_data)
			randloop--;
		txed += (bit9_mode ? i / 2 : i);
                if (txed < old_txed)
                  roll_count++;
                old_txed = txed;
		pkt_count++;
		old_elapsed = elapsed;
		elapsed = time(NULL) - start;
		if (elapsed && ((elapsed != old_elapsed) || fast_updates)) {
			printf("\r%s Transmitted [%2d] %10u bytes at %8d cps",
					etime(elapsed), roll_count, txed,
							(int) (txed / elapsed));
			fflush(stdout);
		}
	}
	printf("\n");
	unlink(FLAG_FILE);
/* fclose(log); */

	return 0;
}
