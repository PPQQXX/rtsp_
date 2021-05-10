#include <stdio.h>
#include <stdlib.h>


void rgb24_split(const char *file, int w, int h, int num) {
    FILE *fp = fopen(file, "r");
    FILE *fr = fopen("output_r.y", "w+");
    FILE *fg = fopen("output_g.y", "w+");
    FILE *fb = fopen("output_b.y", "w+");

    int framesize = w*h*3;      // 一个像素点由3字节组成
    unsigned char buf[framesize];   

    for (int i = 0; i < num; i++) {
        
        fread(buf, 1, framesize, fp); // 一次读一帧图像

        for (int j = 0; j < framesize; j += 3) { 
            // 交替写r,g,b
            fwrite(buf+j, 1, 1, fr);
            fwrite(buf+j+1, 1, 1, fg);
            fwrite(buf+j+2, 1, 1, fb);
        }
    }
    
    fclose(fp);
    fclose(fr);
    fclose(fg);
    fclose(fb);
}


int main(int argc, char const* argv[])
{
    rgb24_split("rgbtest.rgb", 352, 288, 1);
    return 0;
}
