#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;



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

SDL_Surface* convertToRGBA(SDL_Surface* surface) {
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32); //Transformarmos em RGBA32 porque é o formato que conhecemos
    if (!converted) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao converter imagem para RGBA32",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("Falha ao converter para RGBA32: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return NULL;
    }
    return converted;
}

static bool isGreyScale(SDL_Surface* surface) {
    if (!surface) return false;

    //bloquear para manipulação
    if (!SDL_LockSurface(surface)) {
        SDL_Log("isGreyScale: SDL_LockSurface falhou: %s", SDL_GetError());
        return false;
    }

    const int pixeisLargura = surface->w;
    const int pixeisAltura  = surface->h;
    Uint32* pixels = (Uint32*)surface->pixels;
    const int pitchPixels = surface->pitch / 4; // RGBA32

    const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
    const SDL_Palette *palette = SDL_GetSurfacePalette(surface);

    bool isGrey = true;
    for (int y = 0; y < pixeisAltura && isGrey; ++y) {
        for (int x = 0; x < pixeisLargura; ++x) {
            const int idx = y * pitchPixels + x;
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixels[idx], format, palette, &r, &g, &b, &a);
            if (r != g || g != b) {
                isGrey = false;
                break;
            }
        }
    }

    SDL_UnlockSurface(surface);
    return isGrey;
}

SDL_Surface* convertToGrey(SDL_Surface* surface) {
    if (isGreyScale(surface)) {
        SDL_Log("Imagem já está em escala de cinza; pulando conversão.");
        return NULL;
    } else {
        if (!SDL_LockSurface(surface)) { //Precisa bloquear para podermos manipular
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     "Falha ao bloquear superfície",
                                     SDL_GetError(),
                                     NULL);
            SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
            SDL_DestroySurface(surface);
            return NULL;
        }

        SDL_Log("Iniciando Conversão para escala de cinza...");

        const int largura_px = surface->w;
        const int altura_px  = surface->h;
        Uint32* pixels = surface->pixels;
        const int pitchPixels = surface->pitch / 4;
        const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
        const SDL_Palette *palette = SDL_GetSurfacePalette(surface);

        for (int y = 0; y < altura_px; ++y) {
            for (int x = 0; x < largura_px; ++x) {
                const int idx = y * pitchPixels + x;
                Uint8 r, g, b, a;
                SDL_GetRGBA(pixels[idx], format, palette, &r, &g, &b, &a);
                Uint8 gray = (Uint8)(0.2125f * r + 0.7154f * g + 0.0721f * b);
                pixels[idx] = SDL_MapRGBA(format, palette, gray, gray, gray, a);
            }
        }

        SDL_UnlockSurface(surface);
        SDL_Log("Conversão concluida");
        return surface;
    }
}

void shutdown(void) {
    SDL_Log("Shutdown...");
    SDL_Quit();
}

int main(int argc, char *argv[]){
    atexit(shutdown);

    const char* WINDOW_TITLE = "KISHIMOTO EFFECTS";
    float largura;
    float altura;

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("Erro ao iniciar a SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
   // checar se o formato é compativel
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


    SDL_Surface* converted = convertToRGBA(surface);
    surface = convertToGrey(converted);

    largura = surface->w;
    altura = surface->h;

    SDL_CreateWindowAndRenderer(WINDOW_TITLE, largura, altura, 0,&window, &renderer);
    if (!window) {
        SDL_Log("Erro ao criar a janela: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return SDL_APP_FAILURE;
    }
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

        SDL_FRect dest = {0.0f, 0.0f, largura, altura};
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