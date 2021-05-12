#include <stdio.h>
#include "SDL2/SDL.h"


void play_yuv420p(const char *yuvfile, int w, int h) {
    FILE *fp = fopen(yuvfile, "r");
    int yuv_size = w*h*3/2;
    char buf[yuv_size];

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
        printf("failed to init SDL\n");
        goto _Error;
    }

    // 创建窗口
    int screen_w = w;   // 窗口大小
    int screen_h = h;
    SDL_Window *window = SDL_CreateWindow("SDL2.0 Video Sample", \
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, \
            screen_w, screen_h, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        printf("failed to create window.\n");
        goto _Error;
    }

    // 创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        printf("failed to create renderer\n");
        goto _Error;
    }

    // 创建纹理
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, \
            SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);
    if (texture == NULL) {
        printf("failed to create texture\n");
        goto _Error;
    }

    SDL_Rect rect = {
        .x = 0,
        .y = 0,
        .w = screen_w,
        .h = screen_h,
    };


    while (1) {
        if (fread(buf, 1, yuv_size, fp) <= 0) {
            break;
        }

        // yuv420p数据存储时是1字节对齐，使用SDL_UpdateTexture
        SDL_UpdateTexture(texture, &rect, buf, screen_w);
        // 如果是其它对齐方式使用SDL_UpdateYUVTexture函数
        // SDL_UpdateYUVTexture(texture, &rect, \
        //              frame->data[0], frame->linesize[0], \
        //              frame->data[1], frame->linesize[1], \
        //              frame->data[2], frame->linesize[2])

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_RenderPresent(renderer);

        SDL_Delay(40);
    }


_Error:
    if (fp) {
        fclose(fp);
    }
    SDL_Quit();
}


int main(int argc, char const* argv[])
{
    play_yuv420p("../testvideo/output.yuv", 640, 272);    
    return 0;
}
