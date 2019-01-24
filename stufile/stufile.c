#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
    char *fname;
    int fd;
    struct stat stat_buf;
    int ret;
    unsigned char *file_buf;
    int seen_cr;
    int bin_file;
    int n_crlf;
    int n_cr;
    int n_lf;
    int sum;
    float p_crlf;
    float p_cr;
    float p_lf;
    int i;
    int bin_char;
    int bin_index;
    char mixed_string[4096];
    char *mixed_p;

#if 0
{
int i;
for (i = 0; i < argc; i++)
    printf("%d[%s]\n", i, argv[i]);
}
#endif

    if (argc < 2) {
        printf("Missing argument\n");
        return 1;
    }

    fname = argv[1];

    fd = open(fname, O_RDONLY);
//printf("> fd %d\n", fd);
    if (fd == -1) {
        printf("Couldn't open %s, '%s' (%d)\n", fname, strerror(errno), errno);
        return 2;
    }

    ret = fstat(fd, &stat_buf);
//printf("> ret %d\n", ret);
    if (ret == -1) {
        printf("Couldn't fstat %s, '%s' (%d)\n", fname, strerror(errno), errno);
        return 3;
    }
//printf("> size %d\n", stat_buf.st_size);
    if (!stat_buf.st_size) {
        printf("%s: EMPTY file, no line endings\n", fname);
        return 0;
    }

    errno = 0;
    file_buf = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
//printf("> file_buf %d\n", file_buf);
    if (file_buf == (unsigned char *)-1) {
        printf("Couldn't mmap %s, '%s' (%d)\n", fname, strerror(errno), errno);
        return 4;
    }

    n_crlf = 0;
    n_cr = 0;
    n_lf = 0;
    seen_cr = 0;
    bin_file = 0;
    // Skip past Visual Studio signature
    if (stat_buf.st_size >= 3 && (file_buf[0] == 0xef && file_buf[1] == 0xbb && file_buf[2] == 0xbf))
        i = 3;
    else
        i = 0;
    for (; i < stat_buf.st_size; i++) {
        if (file_buf[i] == '\n') {
            if (seen_cr)
                n_crlf++;
            else
                n_lf++;
        } else if (seen_cr)
            n_cr++;

        seen_cr = (file_buf[i] == '\r');

#if 0
        if (stat_buf.st_size - i >= 2 && (file_buf[i] == 0xef && file_buf[i + 1] == 0xbf && file_buf[i + 2] == 0xbd)) {
            // Skip the 3 byte UTF-8 encoded Unicode Replacement Character
            i += 2;
            continue;
        }
#endif

        if (file_buf[i] != '\n' &&
                    file_buf[i] != '\r' &&
                    file_buf[i] != '\t' &&
                    // cp1252 entities: http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT
                    file_buf[i] != 0x91 && // Left Single Quote
                    file_buf[i] != 0x92 && // Right Single Quote
                    file_buf[i] != 0x93 && // Left Double Quote
                    file_buf[i] != 0x94 && // Right Double Quote
                    file_buf[i] != 0x96 && // En Dash
                    file_buf[i] != 0x97 && // Em Dash
                    file_buf[i] != 0xa9 && // Copyright Sign
                    !(file_buf[i] >= ' ' && file_buf[i] <= '~')) {
            bin_file = 1;
            bin_char = file_buf[i];
            bin_index = i;
            break;
        }
    }

    if (bin_file) {
        printf("%s: BINARY file, %#x: %#x\n", fname, bin_index, bin_char);
    } else if ((n_crlf && n_cr) || (n_crlf && n_lf) || (n_cr && n_lf)) {
        sum = n_crlf + n_cr + n_lf;
        p_crlf = ((float)n_crlf / (float)sum) * 100;
        p_cr = ((float)n_cr / (float)sum) * 100;
        p_lf = ((float)n_lf / (float)sum) * 100;
        mixed_p = mixed_string;
        mixed_p += sprintf(mixed_p, "%s: MIXED line endings, Lines: %d", fname, sum);
        if (n_lf)
            mixed_p += sprintf(mixed_p, " LF: %d (%5.2f%%)", n_lf, p_lf);
        if (n_crlf)
            mixed_p += sprintf(mixed_p, " CRLF: %d (%5.2f%%)", n_crlf, p_crlf);
        if (n_cr)
            mixed_p += sprintf(mixed_p, " CR: %d (%5.2f%%)", n_cr, p_cr);
        printf("%s\n", mixed_string);
    } else if (n_crlf) {
        printf("%s: DOS line endings\n", fname);
    } else if (n_cr) {
        printf("%s: MAC line endings\n", fname);
    } else if (n_lf) {
        printf("%s: UNIX line endings\n", fname);
    }

    return 0;
}
