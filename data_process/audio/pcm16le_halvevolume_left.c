#include <stdio.h>
#include <stdlib.h>

#define pcmfile "samples/test.pcm"

int pcm16le_halvevolume_left(char *file) {
    FILE *sfp = fopen(file, "r");
    FILE *dfp = fopen("output_halfleft.pcm","w+");

    int cnt = 0;
    char *samplebuf = malloc(4); // 4Bytes

    while (!feof(sfp)) {
        fread(samplebuf, 1, 4, sfp);

        samplebuf[0] /= 4; // 左声道幅度缩小4倍
        samplebuf[1] /= 4;
        fwrite(samplebuf, 1, 2, dfp);
        fwrite(samplebuf+2, 1, 2, dfp);

        cnt++; // 采样次数
    }

    //printf("sample cnt :%d\n", cnt);

    free(samplebuf);
    fclose(sfp);
    fclose(dfp);
    return 0;
}



int main(int argc, char const* argv[])
{
    pcm16le_halvevolume_left(pcmfile);    
    return 0;
}
