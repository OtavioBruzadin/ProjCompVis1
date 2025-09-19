#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


static const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) return NULL;
    return dot + 1;
}

static bool is_supported_ext(const char* ext) {
    if (!ext) return false;
    const char* exts[] = {"png","jpg","jpeg","bmp","gif","tif","tiff","webp"};
    size_t qtd = sizeof(exts)/sizeof(exts[0]);

    for (size_t i = 0; i < qtd; i++) {
        if (SDL_strcasecmp(ext, exts[i]) == 0) {
            return true;
        }
    }
    return false;
}

void shutdown(void) {
    SDL_Log("Shutdown...");
    SDL_Quit();
}

int main(int argc, char *argv[]){
    atexit(shutdown);

    const char* WINDOW_TITLE = "Hello, SDL";
    const int WINDOW_WIDTH = 640;
    const int WINDOW_HEIGHT = 480;

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("Erro ao iniciar a SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Init Janela
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;

    if (argc < 2) {
        SDL_Log("Uso: %s <imagem.(png|jpg|jpeg|bmp|gif|tif|tiff|webp)>", argv[0]);
        return SDL_APP_FAILURE;
    }

    const char* ext = get_extension(argv[1]);
    if (!is_supported_ext(ext)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Formato não suportado","A imagem informada tem uma extensão não suportada por esta build do SDL_image.\nTente PNG ou JPG ou BMP, por exemplo.",NULL);
        SDL_Log("Formato do arquivo: %s não suportado", argv[1]);
        return SDL_APP_FAILURE;
    }

    SDL_Surface* surface = IMG_Load(argv[1]);
    if (!surface) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao abrir imagem",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("Erro ao carregar imagem '%s': %s", argv[1], SDL_GetError());
        return SDL_APP_FAILURE;
    }
    int largura = surface->w;
    int altura = surface->h;

    window = SDL_CreateWindow(WINDOW_TITLE, largura, altura, 0);
    if (!window) {
        SDL_Log("Erro ao criar a janela: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return SDL_APP_FAILURE;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Erro ao criar o renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroySurface(surface);
        return SDL_APP_FAILURE;
    }

    SDL_Texture* imagem = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (!imagem) {
        SDL_Log("Erro ao criar textura da imagem '%s': %s", argv[1], SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return SDL_APP_FAILURE;
    }

    SDL_Event event;
    bool isRunning = true;
    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                isRunning = false;
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_FRect dest = {0.0f, 0.0f, (float)largura, (float)altura};
        SDL_RenderTexture(renderer, imagem, NULL, &dest);

        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(imagem);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    window = NULL;
    renderer = NULL;
    return 0;
}