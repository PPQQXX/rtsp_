#include <stdio.h>
#include <stdlib.h>

void pcm16le_to_pcm8(char *pcmfile) {
    FILE *sfp = fopen(pcmfile, "r");
    FILE *dfp = fopen("output_pcm8.pcm", "w+");

    char samplebuf[4];

    // pcm16bit --> signed short [-32768, 32767]
    // pcm8bit --> unsigned char [0, 255]
    while (!feof(sfp)) {
        fread(samplebuf, 1, 4, sfp);
        // samplebuf[4]: [aa, bb, 11, 22], 前两个字节为左声道数据

        short temp = *(short *)samplebuf;
        unsigned char left = temp>>8 + 128;  //取高位
        temp = *(short *)(samplebuf+2);
        unsigned char right = temp>>8 + 128;
        fwrite(&left, 1, 1, dfp); // write 8 bit
        fwrite(&right, 1, 1, dfp); // write 8 bit
    }

    fclose(sfp);
    fclose(dfp);
}

int main(int argc, char const* argv[])
{
    pcm16le_to_pcm8("samples/test.pcm"); 
    return 0;
}
