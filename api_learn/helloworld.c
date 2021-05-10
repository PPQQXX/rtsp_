#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "SDL2/SDL.h"

int main(int argc, char **argv) {
    printf("printf ffmpeg configuration: ");
    printf("%s\n", avcodec_configuration());
  
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("could not initialize sdl - %s\n", SDL_GetError());
    }
    SDL_Quit();

    exit(0);
}
