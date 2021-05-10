#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void yuv420_gray(char *yuvfile, int w, int h, int num) {
    FILE *fp = fopen(yuvfile, "r");
    FILE *fgray = fopen("output_gray.yuv", "w+");

    char framebuf[w*h*3/2]; 
    for (int i = 0; i < num; i++) {
        fread(framebuf, 1, w*h*3/2, fp); //取一帧数据

        // y表示明亮度，u，v则是色度、浓度
        // u,v为128时表示灰色
        memset(framebuf + w*h, 128, w*h/2);
        fwrite(framebuf, 1, w*h*3/2, fgray);  
    } 


    fclose(fp);
    fclose(fgray);
}


int main(int argc, char const* argv[])
{
    yuv420_gray("output.yuv", 640, 272, 1); //取一帧数据 
    return 0;
}
