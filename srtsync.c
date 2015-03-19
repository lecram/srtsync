#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define BUFSZ 256

typedef struct Line {
    uint32_t on, off;
    char *text;
} Line;

typedef struct Subtitles {
    int bulk;
    int count;
    Line *lines;
} Subtitles;

uint32_t
ts2ms(const char *ts)
{
    uint32_t ms = 0;
    ms += atoi(ts) * 60 * 60 * 1000;
    ms += atoi(ts + 3) * 60 * 1000;
    ms += atoi(ts + 6) * 1000;
    ms += atoi(ts + 9);
    return ms;
}

uint32_t
hms2ms(const char *hms)
{
    const char *str = hms;
    uint32_t ms = 0;
    while (*str) {
        unsigned long n = strtoul(str, (char **) &str, 10);
        switch (*str) {
        case 'h': case 'H':
            n *= 60;
        case 'm': case 'M':
            n *= 60;
        case 's': case 'S':
            n *= 1000;
        default:
            str++;
        case '\0':
            break;
        }
        ms += n;
    }
    return ms;
}

void
ms2ts(char *buffer, size_t bufsiz, uint32_t ms)
{
    unsigned h, m, s;
    h = ms / (60 * 60 * 1000);
    ms %= 60 * 60 * 1000;
    m = ms / (60 * 1000);
    ms %= 60 * 1000;
    s = ms / (1000);
    ms %= 1000;
    snprintf(buffer, bufsiz, "%02u:%02u:%02u,%03u", h, m, s, ms);
}

Subtitles *
load_subs(FILE *fp)
{
    Subtitles *subs;
    char buffer[BUFSZ];
    char text[4*BUFSZ];
    char *ret = buffer;

    subs = malloc(sizeof(*subs));
    subs->bulk = 256;
    subs->count = 0;
    subs->lines = malloc(subs->bulk * sizeof(*subs->lines));
    while (1) {
        Line line;
        /* Discard blank lines; get index or EOF. */
        strcpy(buffer, "");
        while (ret && strlen(buffer) <= 2)
            ret = fgets(buffer, BUFSZ, fp);
        if (!ret)
            break;
        assert(atoi(buffer) == subs->count + 1);
        /* Get time stamps. */
        fgets(buffer, BUFSZ, fp);
        line.on = ts2ms(buffer);
        line.off = ts2ms(buffer + 17);
        /* Get text. */
        fgets(text, BUFSZ, fp);
        while (1) {
            ret = fgets(buffer, BUFSZ, fp);
            if (ret && strlen(buffer) > 2)
                strcat(text, buffer);
            else
                break;
        }
        line.text = malloc(strlen(text) + 1);
        strcpy(line.text, text);
        /* Add line to subtitles. */
        if (subs->count == subs->bulk) {
            subs->bulk += subs->bulk / 2;
            subs->lines = realloc(subs->lines, subs->bulk * sizeof(*subs->lines));
        }
        subs->lines[subs->count++] = line;
    }
    return subs;
}

void
print_line(FILE *fp, Subtitles *subs, int index)
{
    Line *line;
    char bufon[13], bufoff[13];

    fprintf(fp, "%d\r\n", index + 1);
    line = subs->lines + index;
    ms2ts(bufon, 13, line->on);
    ms2ts(bufoff, 13, line->off);
    fprintf(fp, "%s --> %s\r\n", bufon, bufoff);
    fprintf(fp, "%s", line->text);
}

void
save_subs(FILE *fp, Subtitles *subs)
{
    int i;

    for (i = 0; i < subs->count; i++) {
        print_line(fp, subs, i);
        fprintf(fp, "\r\n");
    }
}

void
free_subs(Subtitles **subs)
{
    int i;

    for (i = 0; i < (*subs)->count; i++)
        free((*subs)->lines[i].text);
    free((*subs)->lines);
    free(*subs);
    *subs = NULL;
}

void
transform(Subtitles *subs, double factor, int sign, uint32_t offset)
{
    int i;

    for (i = 0; i < subs->count; i++) {
        subs->lines[i].on = subs->lines[i].on * factor + sign * offset;
        subs->lines[i].off = subs->lines[i].off * factor + sign * offset;
    }
}

void
sync(Subtitles *subs, int i1, uint32_t t1, int i2, uint32_t t2)
{
    double factor;
    int32_t shift;
    int sign;
    uint32_t offset;
    uint32_t t1_old = subs->lines[i1-1].on;
    uint32_t t2_old = subs->lines[i2-1].on;
    factor = ((double) (t2 - t1)) / (t2_old - t1_old);
    shift = t1 - t1_old * factor;
    fprintf(stderr, "scaled by %g, shifted by %gs\n", factor, shift / 1e3);
    sign = shift < 0 ? -1 : +1;
    offset = abs(shift);
    transform(subs, factor, sign, offset);
}

int
closest(Subtitles *subs, uint32_t ms)
{
    int imin, imax;

    imin = 0;
    imax = subs->count - 1;
    do {
        int i = (imin + imax) / 2;
        uint32_t sms = subs->lines[i].on;
        if (sms < ms)
            imin = i + 1;
        else if (sms > ms)
            imax = i - 1;
        else
            return i;
    } while (imin < imax);
    return imin;
}

int
contains(Line *line, char *words[], int nwords)
{
    char *str = line->text;
    while (nwords--)
        if (!(str = strstr(str, *words++)))
            return 0;
    return 1;
}

int
search(Subtitles *subs, uint32_t ms, char *words[], int nwords)
{
#define SRT_CHECK(I) if (contains(&subs->lines[I], words, nwords)) return I
    int i, s, t;

    i = closest(subs, ms);
    SRT_CHECK(i);
    s = 1;
    while (i-s > 0 && i+s < subs->count) {
        SRT_CHECK(i-s);
        SRT_CHECK(i+s);
        s++;
    }
    for (t = i-s; t >= 0; t--)
        SRT_CHECK(t);
    for (t = i+s; t < subs->count; t++)
        SRT_CHECK(t);
    return -1;
#undef SRT_CHECK
}

void
usage(FILE *fp)
{
    fprintf(fp,
        "usage:\n"
        "  srtsync (-h|--help|help) -- print this help message\n"
        "  srtsync search TIME [WORD [WORD [...]]] -- search around TIME\n"
        "  srtsync shift (-TIME|+TIME) -- shift all subtitles by TIME\n"
        "  srtsync scale FACTOR -- multiply all time stamps by FACTOR\n"
        "  srtsync sync INDEX TIME INDEX TIME -- linearly sync subtitles\n"
        "\n"
    );
}

int
main(int argc, char *argv[])
{
    Subtitles *subs;

    if (argc == 1) {
        usage(stderr);
        return 1;
    }
    if (!(strcmp(argv[1], "-h") && strcmp(argv[1], "--help") && strcmp(argv[1], "help"))) {
        usage(stdout);
        return 0;
    }
    subs = load_subs(stdin);
    if (!strcmp(argv[1], "search") && argc >= 3) {
        int i;
        i = search(subs, hms2ms(argv[2]), argv + 3, argc - 3);
        if (i >= 0) {
            print_line(stdout, subs, i);
            return 0;
        } else {
            fprintf(stderr, "Not found.\n");
            return 1;
        }
    } else if (!strcmp(argv[1], "shift") && argc == 3) {
        int sign;
        uint32_t offset;
        char *hms = argv[2];
        switch (*hms) {
        case '-':
            hms++;
            sign = -1;
            break;
        case '+':
            hms++;
        default:
            sign = +1;
        }
        offset = hms2ms(hms);
        transform(subs, 1, sign, offset);
    } else if (!strcmp(argv[1], "scale") && argc == 3) {
        double factor = atof(argv[2]);
        transform(subs, factor, 0, 0);
    } else if (!strcmp(argv[1], "sync") && argc == 6) {
        int i1 = atoi(argv[2]);
        uint32_t t1 = hms2ms(argv[3]);
        int i2 = atoi(argv[4]);
        uint32_t t2 = hms2ms(argv[5]);
        sync(subs, i1, t1, i2, t2);
    } else {
        usage(stderr);
        return 1;
    }
    save_subs(stdout, subs);
    free_subs(&subs);
    return 0;
}
