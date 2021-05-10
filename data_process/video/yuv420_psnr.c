#include <stdio.h>
#include <math.h>

// 对比两张图像的Y分量
void yuv420_psnr(char *origin, char *target, int w, int h, int num) {
    FILE *fp = fopen(origin, "r");
    FILE *ft = fopen(target, "r");

    unsigned char buf1[w*h]; 
    unsigned char buf2[w*h]; 
    float psnr = 0.0;
    for (int i = 0; i < num; i++) {
        fread(buf1, 1, w*h, fp);
        fread(buf2, 1, w*h, ft);

        int sum = 0;
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                int t = i*w + j;
                sum += pow((buf1[t] - buf2[t]),2);
            }
        }
        float MSE = (float)sum / (float)(w*h);
        
        psnr = 10*log10(255*255/MSE);
        printf("psnr: %f\n", psnr);

        fseek(fp, w*h/2, SEEK_CUR);
        fseek(ft, w*h/2, SEEK_CUR);
    } 


    fclose(fp);
    fclose(ft);
}


int main(int argc, char const* argv[])
{
    yuv420_psnr("origin.yuv", "target.yuv", 640, 272, 1); //取一帧数据 
    return 0;
}
