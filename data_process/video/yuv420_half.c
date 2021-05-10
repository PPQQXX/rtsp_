#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void yuv420_half(char *yuvfile, int w, int h, int num) {
    FILE *fp = fopen(yuvfile, "r");
    FILE *fhalf = fopen("output_half.yuv", "w+");

    unsigned char framebuf[w*h*3/2]; 
    for (int i = 0; i < num; i++) {
        fread(framebuf, 1, w*h*3/2, fp); //取一帧数据
        
        for (int j = 0; j < w*h; j++) {
            framebuf[j] >>= 1;  // 缩小y分量值
        }

        fwrite(framebuf, 1, w*h*3/2, fhalf);  
    } 


    fclose(fp);
    fclose(fhalf);
}


int main(int argc, char const* argv[])
{
    yuv420_half("output_gray.yuv", 640, 272, 1); //取一帧数据 
    return 0;
}
