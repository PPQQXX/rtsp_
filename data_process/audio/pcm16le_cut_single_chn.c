#include <stdio.h>
#include <stdlib.h>

void pcm16le_cut_single_chn(char *file, int start, int duration) {
    FILE *fp = fopen(file, "r");
    FILE *dfp = fopen("output_cut.pcm", "w+");
    FILE *stat = fopen("output_cut.txt", "w+");

    unsigned char sample[4];
    int cnt = 0;
    
    // 从单声道数据中截取一段数据，并输出截取数据的样值
    while (!feof(fp)) {
        fread(sample, 1, 2, fp);
        if (cnt >= start && cnt < start + duration) {
            fwrite(sample, 1, 2, dfp);

            short temp = ((short)sample[1])<<8 + sample[0];
            fprintf(stat, "%6d", temp);
            if (cnt % 10 == 0) {
                fprintf(stat,"\n");
            }
        }
        cnt++;
    }


    fclose(fp);
    fclose(dfp);
    fclose(stat);
}

int main(int argc, char const* argv[])
{
    // 44100hz, 单声道，88w个点，20秒视频
    // 从第2000个采样点开始截取，截取8w个点, 约1s
    pcm16le_cut_single_chn("samples/single.pcm", 2000, 80000); 
    return 0;
}
