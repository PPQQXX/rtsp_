#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void yuv420_split(char *yuvfile, int w, int h, int num) {
    FILE *fp = fopen(yuvfile, "r");
    FILE *fy = fopen("output_420_y.y", "w+");
    FILE *fu = fopen("output_420_u.y", "w+");
    FILE *fv = fopen("output_420_v.y", "w+");

    char *framebuf = malloc(sizeof(char) * w*h*3);    // yuv444 --> y:u:v = 1:1:1
    for (int i = 0; i < num; i++) {
        fread(framebuf, 1, w*h*3, fp); //取一帧数据

        fwrite(framebuf, 1, w*h, fy);  // y
        fwrite(framebuf + w*h, 1, w*h, fu);  // u
        fwrite(framebuf + w*h*2 , 1, w*h, fv);  // v
    } 
    
    free(framebuf);
    fclose(fp);
    fclose(fy);
    fclose(fu);
    fclose(fv);
}


int main(int argc, char const* argv[])
{
    yuv420_split("test_yuv444.yuv", 640, 272, 1); //取一帧数据 
    return 0;
}
