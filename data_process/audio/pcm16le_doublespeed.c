#include <stdio.h>
#include <stdlib.h>

void pcm16le_doublespeed(char *file) {
    FILE *sfp = fopen(file, "r");
    FILE *dfp = fopen("doublespeed.pcm", "w+");

    int cnt = 0;
    char *samplebuf = malloc(4); //4Bytes

    while (!feof(sfp)) {
        fread(samplebuf, 1, 4, sfp);

        // 原来是44100KHZ，20秒视频
        // 两倍速, 间隔采样，视频时长减半
        if (cnt % 2 != 0) {
            fwrite(samplebuf, 1, 2, dfp);
            fwrite(samplebuf+2, 1, 2, dfp);
        }
        cnt++;
    }
    printf("sample cnt :%d\n", cnt);
    free(samplebuf);
    fclose(sfp);
    fclose(dfp);
}

int main(int argc, char const* argv[])
{
    pcm16le_doublespeed("samples/test.pcm");
    return 0;
}
