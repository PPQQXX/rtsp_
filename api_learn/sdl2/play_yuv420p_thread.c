#include <stdio.h>
#include "SDL2/SDL.h"

#define REFRESH_EVENT (SDL_USEREVENT + 0)
#define STOP_EVENT    (SDL_USEREVENT + 1)
#define QUIT_EVENT    (SDL_USEREVENT + 2)

// 事件驱动，多线程
int thread_quit = 0;
int thread_stop = 0;

// 刷新线程：40ms刷新一次事件，正常会一般播放
// 发生按键操作时，会将播放主线程暂停或退出
int refresh_thread(void *p) {
    SDL_Event event;
    while (!thread_quit) {
        if (!thread_stop) {
            event.type = REFRESH_EVENT;
        } else {
            event.type = STOP_EVENT;
        }
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }
    event.type = QUIT_EVENT;
    SDL_PushEvent(&event);
    return 0;
}

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
    
    SDL_CreateThread(refresh_thread, "video thread", NULL);

    SDL_Event event;
    while (1) {
        if (event.type == SDL_KEYDOWN) {
            // 空格键暂停，ESC键退出
            if (event.key.keysym.sym == SDLK_SPACE) {
                thread_stop = !thread_stop;
            } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                thread_quit = 1;
            }
        } 
        
        SDL_WaitEvent(&event);
        if (event.type == REFRESH_EVENT) {
            if (!thread_stop) {
                if (fread(buf, 1, yuv_size, fp) <= 0) {
                    break;
                }

                SDL_UpdateTexture(texture, &rect, buf, screen_w);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &rect);
                SDL_RenderPresent(renderer);
            }
        } else if (event.type == QUIT_EVENT) {
            break;
        } else {   // STOP_EVENT 
            // do nothing...
        }
    }

_Error:
    if (fp) {
        fclose(fp);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    SDL_Quit();
}


int main(int argc, char const* argv[])
{
    play_yuv420p("../testvideo/output.yuv", 640, 272);    
    return 0;
}
