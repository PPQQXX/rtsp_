#include <stdio.h>
#include <stdlib.h>


unsigned char clip_0_255(int val) {
    if (val < 0) return 0;
    else if (val > 255) return 255;
    return (unsigned char)val;
}

// yuv240p的存储格式是planar，而rgb24的存储格式是packed
void data_conversion(unsigned char *yuvbuf, int w, int h, unsigned char *rgbbuf) {
    unsigned char *pY = yuvbuf;
    unsigned char *pU = yuvbuf + w*h;
    unsigned char *pV = pU + w*h/4;

    int cnt = 0;
    int yIdx, uIdx, vIdx;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            yIdx = i*w + j;
            uIdx = vIdx = (i/4)*w + j/2;

            int R = (298*pY[yIdx] + 411*pV[vIdx] - 57344)>>8;
            int G = (298*pY[yIdx] - 101*pU[uIdx] - 211*pV[vIdx] + 34739)>>8;
            int B = (298*pY[yIdx] + 519*pU[uIdx] - 71117)>>8;
            
            rgbbuf[cnt++] = clip_0_255(R);
            rgbbuf[cnt++] = clip_0_255(G);
            rgbbuf[cnt++] = clip_0_255(B);
        }
    }
}


void rgb24_to_yuv420(const char *yuvfile, int w, int h, const char *rgbfile) {
    FILE *fyuv = fopen(yuvfile, "r");
    FILE *frgb = fopen(rgbfile, "w+");

    // 同样表示4个像素点
    // yuv --> yyyy u v  = 6字节
    // rgb --> (r,g,b)*4 = 12字节
    int rgbsize = w*h*3, yuvsize = w*h*3/2;
    unsigned char rgbbuf[rgbsize];
    unsigned char yuvbuf[yuvsize];
    fread(yuvbuf, 1, yuvsize, fyuv);
    
    data_conversion(yuvbuf, w, h, rgbbuf);
    fwrite(rgbbuf, 1, rgbsize, frgb);

    fclose(frgb);
    fclose(fyuv);
}



int main(int argc, char const* argv[])
{
    rgb24_to_yuv420("yuvtest.yuv", 352, 288, "yuvtorgb.rgb"); 
    return 0;
}
