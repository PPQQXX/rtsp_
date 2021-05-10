#include <stdio.h>
#include <stdlib.h>


unsigned char clip_0_255(int val) {
    if (val < 0) return 0;
    else if (val > 255) return 255;
    return (unsigned char)val;
}

// yuv240p的存储格式是planar，而rgb24的存储格式是packed
void data_conversion(unsigned char *rgbbuf, int w, int h, unsigned char *yuvbuf) {
    unsigned char *pY = yuvbuf;
    unsigned char *pU = yuvbuf + w*h;
    unsigned char *pV = pU + w*h/4;

    unsigned char r,g,b;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int start = (i*w + j)*3;
            // 注意r分量存储在低字节，b分量存储在高字节
            r = rgbbuf[start]; 
            g = rgbbuf[start+1]; 
            b = rgbbuf[start+2];

            int y = ((66*r + 129*g + 25*b + 128)>>8) + 16;
            *pY++ = clip_0_255(y);

            //采样2个y时取1个u或v
            if (i % 2 == 0 && j % 2 == 0) { // uv的采样在水平和垂直方向都是p的一半
                int u = ((-38*r - 74*g + 112*b + 128)>>8) + 128;
                *pU++ =  clip_0_255(u);
            }
            else if (i % 2 == 0 && j % 2 == 1) {
                int v = ((112*r - 94*g - 18*b + 128)>>8) + 128;
                *pV++ =  clip_0_255(v);
            }
        }
    }
}


void rgb24_to_yuv420(const char *rgbfile, int w, int h, const char *yuvfile) {
    FILE *frgb = fopen(rgbfile, "r");
    FILE *fyuv = fopen(yuvfile, "w+");

    // 同样表示4个像素点
    // yuv --> yyyy u v  = 6字节
    // rgb --> (r,g,b)*4 = 12字节
    int rgbsize = w*h*3, yuvsize = w*h*3/2;
    unsigned char rgbbuf[rgbsize];
    unsigned char yuvbuf[yuvsize];
    fread(rgbbuf, 1, rgbsize, frgb);
    
    data_conversion(rgbbuf, w, h, yuvbuf);
    fwrite(yuvbuf, 1, yuvsize, fyuv);


    fclose(frgb);
    fclose(fyuv);
}



int main(int argc, char const* argv[])
{
    rgb24_to_yuv420("rgbtest.rgb", 352, 288, "rgbtoyuv.yuv"); 
    return 0;
}
