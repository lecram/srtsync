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
load_subs(const char *path)
{
    Subtitles *subs;
    FILE *fp;
    char buffer[BUFSZ];
    char text[4*BUFSZ];
    char *ret = buffer;

    fp = fopen(path, "r");
    if (!fp)
        return NULL;
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
            subs->bulk += subs->bulk >> 1;
            subs->lines = realloc(subs->lines, subs->bulk * sizeof(*subs->lines));
        }
        subs->lines[subs->count++] = line;
    }
    fclose(fp);
    return subs;
}

void
save_subs(Subtitles *subs, const char *path)
{
    FILE *fp;
    char bufon[13], bufoff[13];
    int i;

    fp = fopen(path, "w");
    if (!fp)
        return;
    for (i = 0; i < subs->count; i++) {
        Line line = subs->lines[i];
        fprintf(fp, "%d\r\n", i + 1);
        ms2ts(bufon, 13, line.on);
        ms2ts(bufoff, 13, line.off);
        fprintf(fp, "%s --> %s\r\n", bufon, bufoff);
        fprintf(fp, "%s\r\n", line.text);
    }
    fclose(fp);
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

int
main(int argc, char *argv[])
{
    Subtitles *subs;
    assert(argc == 2);
    subs = load_subs(argv[1]);
    save_subs(subs, "test.srt");
    free_subs(&subs);
    return 0;
}
