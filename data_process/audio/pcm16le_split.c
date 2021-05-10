#include <stdio.h>
#include <stdlib.h>

// 将PCM16LE双声道数据中左右声道的数据分离成两个文件
int pcm16le_split(char *pcmfile) {
    
    FILE *sfp = fopen(pcmfile, "r"); // read only
    FILE *dfp1 = fopen("output_l.pcm", "w+"); //read and write
    FILE *dfp2 = fopen("output_r.pcm", "w+");

    char *sample = malloc(sizeof(char) * 4); // 4Bytes

    while (!feof(sfp)) { // end of file
        fread(sample, sizeof(char), 4, sfp);
        fwrite(sample, sizeof(char), 2, dfp1); //L
        fwrite(sample + 2, sizeof(char), 2, dfp2); //R
    }

    free(sample);
    fclose(sfp);
    fclose(dfp1);
    fclose(dfp2);
    return 0;
}


int main(int argc, char const* argv[])
{
    pcm16le_split("samples/test.pcm");
    return 0;
}
