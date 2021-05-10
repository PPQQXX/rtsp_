#include <stdio.h>
#include <stdlib.h>

// bmp文件结构：
// 位图文件头(14字节)：{'B','M'} + fsize + blank + offset
// 位图信息头(40字节)：信息头大小，图像宽高、色深，压缩方式，图像数据大小，，，
typedef struct {
    int imageSize;  // 图像文件大小
    int blank;      // 全为0
    int offset;     // 图像数据的地址
} bmpHead;

typedef struct {
    int Length;     // 结构体大小
    int width;      // 分辨率宽度    
    int height;     // 分辨率高度
    unsigned short colorPlane;  // 目标设备的级别，必须为1
    unsigned short bitColor;    // 颜色深度，即每个像素所需要位数
    int compression;// 压缩方式，0表示不压缩
    int realSize;   // 图像数据大小
    int xPels;      // 水平分辨率，单位像素/m
    int yPels;      // 垂直分辨率
    int colorUse;   // bmp图像使用的颜色，0表示全部颜色
    int colorImportant;         // 重要的颜色数，0时所有颜色都重要
} infoHead;


#define swap(a,b) {unsigned char t = a; a = b; b = t;}
// BMP采用小端存储方式，RGB分量存储顺序为BGR
void rgb24_to_bmp(const char *rgbfile, int w, int h, const char *bmpfile) {
    FILE *frgb = fopen(rgbfile, "r");
    FILE *fbmp = fopen(bmpfile, "w+"); 
    
    int rgbsize = w*h*3;
    unsigned char rgbbuf[rgbsize];

    // 填充Bmp文件头和信息头
    char bmpfileType[2] = {'B', 'M'};
    int headersize = sizeof(bmpfileType) + sizeof(bmpHead) + sizeof(infoHead);
    bmpHead bmpHeader;
    bmpHeader.imageSize = headersize + rgbsize;
    bmpHeader.blank = 0;
    bmpHeader.offset = headersize;


    infoHead infoHeader;
    infoHeader.Length = sizeof(infoHead);
    infoHeader.width = w;
    infoHeader.height = -h;         // bmp从底到顶加载像素数据
    infoHeader.colorPlane = 1;
    infoHeader.bitColor = 24;
    infoHeader.compression = 0;
    infoHeader.realSize = rgbsize;  // 或width*height*bitColor/8
    infoHeader.colorUse = 0;
    infoHeader.colorImportant = 0;

    fwrite(bmpfileType, 1, sizeof(bmpfileType), fbmp);
    fwrite(&bmpHeader, 1, sizeof(bmpHeader), fbmp);
    fwrite(&infoHeader, 1, sizeof(infoHeader), fbmp);


    // 调整rgb分量存储顺序
    fread(rgbbuf, 1, rgbsize, frgb);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int start = (i*w + j)*3;
            swap(rgbbuf[start], rgbbuf[start+2]);
        }
    }
    // 填充图像数据
    fwrite(rgbbuf, 1, rgbsize, fbmp);

    fclose(frgb); 
    fclose(fbmp);
}


int main(int argc, char const* argv[])
{
    rgb24_to_bmp("rgbtest.rgb", 352, 288, "output_bmp.bmp"); 
    return 0;
}
