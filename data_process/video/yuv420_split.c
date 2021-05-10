#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void yuv420_split(char *yuvfile, int w, int h, int num) {
    FILE *fp = fopen(yuvfile, "r");
    FILE *fy = fopen("output_420_y.y", "w+");
    FILE *fu = fopen("output_420_u.y", "w+");
    FILE *fv = fopen("output_420_v.y", "w+");

    char framebuf[w*h*3/2];    // yuv420 --> y 1Bytes u&v 0.5Bytes
    for (int i = 0; i < num; i++) {
        fread(framebuf, 1, w*h*3/2, fp); //取一帧数据

        fwrite(framebuf, 1, w*h, fy);  // y
        fwrite(framebuf + w*h, 1, w*h/4, fu);  // u
        fwrite(framebuf + w*h + w*h/4 , 1, w*h/4, fv);  // v
    } 


    fclose(fp);
    fclose(fy);
    fclose(fu);
    fclose(fv);
}


int main(int argc, char const* argv[])
{
    yuv420_split("output.yuv", 640, 272, 1); //取一帧数据 
    return 0;
}
