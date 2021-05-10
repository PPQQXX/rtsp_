#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void yuv420_border(char *yuvfile, int w, int h, int border, int num) {
    FILE *fp = fopen(yuvfile, "r");
    FILE *fborder = fopen("output_border.yuv", "w+");

    unsigned char framebuf[w*h*3/2]; 
    for (int i = 0; i < num; i++) {
        fread(framebuf, 1, w*h*3/2, fp); // 取一帧数据
        
        for (int j = 0; j < h; j++) {    // 行扫描
            for (int k = 0; k < w; k++) {// 列扫描
                if (j < border || j > h - border || \
                        k < border || k > w - border) 
                framebuf[j*w + k] = 255; // 白边框
            }
        }

        fwrite(framebuf, 1, w*h*3/2, fborder);  
    } 


    fclose(fp);
    fclose(fborder);
}


int main(int argc, char const* argv[])
{
    yuv420_border("output_gray.yuv", 640, 272, 20, 1); //取一帧数据 
    return 0;
}
