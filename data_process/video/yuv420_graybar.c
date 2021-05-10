#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 生成灰阶测试图, barnum指有多少个亮度层次
void yuv420_graybar(int w, int h, int ymin, int ymax, int barnum) {
    FILE *fout = fopen("output_graybar.yuv", "w+");
    
    unsigned char data_y[w*h];
    unsigned char data_u[w*h/4];
    unsigned char data_v[w*h/4];

    unsigned char lums[barnum];
    float lum_inc = (float)(ymax - ymin) / (barnum - 1);
    for (int i = 0; i < barnum; i++) {
        lums[i] = ymin + (unsigned char)(i * lum_inc);
    }
    
    unsigned char level[w];
    int barwidth = w / barnum;     
    for (int i = 0; i < barnum; i++) {
        memset(level + i*barwidth, lums[i], barwidth);
    }
    
    // draw picture
    for (int i = 0; i < h; i++) {
        memcpy(data_y + i*w, level, w);
    }
    memset(data_u, 128, w*h/4);
    memset(data_v, 128, w*h/4);
   
    fwrite(data_y, w*h, 1, fout);
    fwrite(data_u, w*h/4, 1, fout);
    fwrite(data_v, w*h/4, 1, fout);
    
    fclose(fout);
}

int main(int argc, char const* argv[])
{
    yuv420_graybar(320, 240, 0, 255, 10);
    return 0;
}
