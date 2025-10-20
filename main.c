#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

SDL_Window *window2 = NULL;
SDL_Renderer *renderer2 = NULL;

typedef struct ImageData ImageData;
struct ImageData{
    SDL_Texture* intensityTexture;
    SDL_FRect    intensityRect;
    SDL_Texture* deviationTexture;
    SDL_FRect    deviationRect;
};

typedef struct {
    SDL_FRect rect;
    bool hovered;
    bool pressed;
} Button;

typedef enum { VIEW_ORIGINAL = 0, VIEW_EQUALIZED, VIEW_PSEUDOCOLOR } ViewMode;
typedef enum { COLORMAP_JET = 0, COLORMAP_HOT, COLORMAP_VIRIDIS, COLORMAP_COUNT } ColormapType;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline void jet_map(uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
    float x = v / 255.0f;
    float R = clampf(1.5f - fabsf(4.0f*x - 3.0f), 0.0f, 1.0f);
    float G = clampf(1.5f - fabsf(4.0f*x - 2.0f), 0.0f, 1.0f);
    float B = clampf(1.5f - fabsf(4.0f*x - 1.0f), 0.0f, 1.0f);
    *r = (uint8_t)(R * 255.0f);
    *g = (uint8_t)(G * 255.0f);
    *b = (uint8_t)(B * 255.0f);
}

static inline void hot_map(uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
    float x = v / 255.0f;
    float R = clampf(3.0f * x,       0.0f, 1.0f);
    float G = clampf(3.0f * x - 1.0f,0.0f, 1.0f);
    float B = clampf(3.0f * x - 2.0f,0.0f, 1.0f);
    *r = (uint8_t)(R * 255.0f);
    *g = (uint8_t)(G * 255.0f);
    *b = (uint8_t)(B * 255.0f);
}

static inline void viridis_map(uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
    static const struct { float x; uint8_t r,g,b; } P[] = {
        {0.00f,  68,  1, 84},
        {0.33f,  59, 82,139},
        {0.66f,  33,144,141},
        {1.00f, 253,231, 37}
    };
    float x = v / 255.0f;
    int i = 0;
    while (i < 3 && x > P[i+1].x) i++;
    float t = (x - P[i].x) / (P[i+1].x - P[i].x);
    *r = (uint8_t)((1.0f - t)*P[i].r + t*P[i+1].r);
    *g = (uint8_t)((1.0f - t)*P[i].g + t*P[i+1].g);
    *b = (uint8_t)((1.0f - t)*P[i].b + t*P[i+1].b);
}

static inline void map_gray_to_rgb(uint8_t gray, ColormapType cm, uint8_t* r, uint8_t* g, uint8_t* b) {
    switch (cm) {
        case COLORMAP_JET:     jet_map(gray, r, g, b);     break;
        case COLORMAP_HOT:     hot_map(gray, r, g, b);     break;
        case COLORMAP_VIRIDIS: viridis_map(gray, r, g, b); break;
        default:               jet_map(gray, r, g, b);     break;
    }
}

static const char* colormapName(ColormapType cm) {
    switch (cm) {
        case COLORMAP_JET: return "JET";
        case COLORMAP_HOT: return "HOT";
        case COLORMAP_VIRIDIS: return "VIRIDIS";
        default: return "JET";
    }
}

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
    SDL_Log("Iniciando Conversão para RGBA...");

    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (!converted) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao converter imagem para RGBA32",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("Falha ao converter para RGBA32: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return NULL;
    }
    SDL_Log("Conversão para RGBA concluida");

    return converted;
}

static bool isGreyScale(SDL_Surface* surface) {
    if (!surface) return false;

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
    if (!surface) {
        SDL_Log("convertToGrey: superfície nula recebida.");
        return NULL;
    }

    if (isGreyScale(surface)) {
        SDL_Log("Imagem já está em escala de cinza; pulando conversão e mantendo superfície.");
        return surface; // mantém a superfície para uso posterior
    }

    if (!SDL_LockSurface(surface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        return NULL;
    }

    SDL_Log("Iniciando Conversão para escala de cinza...");

    const int largura_px = surface->w;
    const int altura_px  = surface->h;
    Uint32* pixels = (Uint32*)surface->pixels;
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
    SDL_Log("Conversão para escala de cinza concluída");
    return surface;
}

void obtainIntesityFrequency (SDL_Surface* surface, int* intensityFrequency){

    if (!SDL_LockSurface(surface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        return;
    }

    for (int i = 0; i < 256; ++i) intensityFrequency[i] = 0;

    const size_t pixelCount = surface->w * surface->h;
    const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);

    Uint32 *pixels = (Uint32 *)surface->pixels;
    Uint8 r = 0, g = 0, b = 0, a = 0;

    for (size_t i = 0; i < pixelCount; ++i)
    {
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

        if (!(r == g && r == b && g == b)){
            SDL_Log("Imagem nao esta em escala de cinza");
            SDL_UnlockSurface(surface);
            return;
        } else {
            intensityFrequency[r]++;
        }
    }

    SDL_UnlockSurface(surface);
}

void createHistogram(SDL_Surface* surface) {

    if (!surface) {
        SDL_Log("Erro! Superficie Invalida.");
        return;
    }

    int intensityFrequency[256];
    obtainIntesityFrequency(surface, intensityFrequency);

    float normalizedIntensity[256];
    for (int i = 0; i < 256; ++i) normalizedIntensity[i] = 0.0f;

    const size_t pixelCount = surface->w * surface->h;
    for (int i = 0; i < 256; i++)
        normalizedIntensity[i] = (float)intensityFrequency[i]/pixelCount;

    SDL_SetRenderDrawColor(renderer2, 255, 255, 255, 255);

    SDL_FRect rect1 = { .x = 2.0f, .y = 2.0f, .w = (420.0f-4.0f), .h = (500.0f-4.0f)};
    SDL_RenderRect(renderer2, &rect1);

    SDL_FRect rect2 = { .x = 4.0f, .y = 4.0f, .w = (420.0f-8.0f), .h = (500.0f-8.0f)};
    SDL_RenderRect(renderer2, &rect2);

    SDL_FRect rect3 = { .x = 6.0f, .y = 6.0f, .w = (420.0f-12.0f), .h = (500.0f-12.0f)};
    SDL_RenderRect(renderer2, &rect3);

    SDL_FRect rect4 = { .x = 8.0f, .y = 8.0f, .w = (420.0f-16.0f), .h = (500.0f-16.0f)};
    SDL_RenderRect(renderer2, &rect4);

    SDL_FRect rect = { .x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 0.0f };

    rect.x = 10.0f; rect.y = 10.0f; rect.w = 420.0f - 20.0f; rect.h = 500.0f - 20.0f;
    SDL_RenderRect(renderer2, &rect);

    rect.x = 80.0f; rect.y = 40.0f; rect.w = 257.0f; rect.h = 257.0f;
    SDL_RenderRect(renderer2, &rect);

    SDL_SetRenderDrawColor(renderer2, 255, 255, 255, 255);

    float maxFrequency = 0.0f;
    for (int i = 0; i <= 255; i++)
        if (maxFrequency < normalizedIntensity[i]) maxFrequency = normalizedIntensity[i];

    for (int i = 0; i <= 255; i++){
        if (normalizedIntensity[i] > 0.0f){
            SDL_RenderLine(renderer2, (80.0f + (i+1)), (41.0f + 255.0f),
                           (80.0f + (i+1)), ((41.0f + 255.0f)-((normalizedIntensity[i]/(maxFrequency))*256.0f)));
        }
    }
}

char* obtainImageClass(SDL_Surface* surface, int* intensityFrequency) {

    int soma = 0;
    for (int i = 0; i <= 255; i++) soma += i*intensityFrequency[i];

    const size_t pixelCount = surface->w * surface->h;
    float media = (float)soma/pixelCount;

    if (media < 85.0f)      return "Escura";
    else if (media > 170.0f) return "Clara";
    else                     return "Média";
}

char* obtainImageDeviation(SDL_Surface* surface, int* intensityFrequency) {

    int min = 300;
    int max = -1;

    for (int i = 0; i <= 255; i++){
        if(intensityFrequency[i] > 0){
            if(i < min) min = i;
            if(i > max) max = i;
        }
    }

    int var = abs(max-min);
    if (var < 85)      return "Baixo";
    else if (var > 170) return "Alto";
    else               return "Médio";
}

void analyzeImageInformation(SDL_Surface* surface, ImageData* data, TTF_Font* font){

    int intensityFrequency[256];
    SDL_Color textColor = { 255, 255, 255, 255 };

    obtainIntesityFrequency(surface, intensityFrequency);

    char* imageClass = obtainImageClass(surface, intensityFrequency);
    char classe[35];
    snprintf(classe, 35, "Média de Intensidade: %s", imageClass);

    SDL_Surface* textIntensitySurface = TTF_RenderText_Blended(font, classe, 0, textColor);
    if (!textIntensitySurface) { SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError()); return; }

    data->intensityTexture = SDL_CreateTextureFromSurface(renderer2, textIntensitySurface);
    if (!data->intensityTexture) { SDL_Log("Erro ao criar a textura: %s", SDL_GetError()); return; }

    data->intensityRect.w = textIntensitySurface->w;
    data->intensityRect.h = textIntensitySurface->h;
    data->intensityRect.x = 40.0f;
    data->intensityRect.y = 340.0f;
    SDL_DestroySurface(textIntensitySurface);

    char* imageDeviation = obtainImageDeviation(surface, intensityFrequency);
    char deviation[30];
    snprintf(deviation, 30, "Desvio Padrão: %s", imageDeviation);

    SDL_Surface* textDeviationSurface = TTF_RenderText_Blended(font, deviation, 0, textColor);
    if (!textDeviationSurface) { SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError()); return; }

    data->deviationTexture = SDL_CreateTextureFromSurface(renderer2, textDeviationSurface);
    if (!data->deviationTexture) { SDL_Log("Erro ao criar a textura: %s", SDL_GetError()); return; }

    data->deviationRect.w = textDeviationSurface->w;
    data->deviationRect.h = textDeviationSurface->h;
    data->deviationRect.x = 40.0f;
    data->deviationRect.y = 380.0f;
    SDL_DestroySurface(textDeviationSurface);
}

void destroyData(ImageData* data){
    if (!data) { SDL_Log("Não há informações disponíveis da imagem!"); return; }
    SDL_DestroyTexture(data->intensityTexture);
    SDL_DestroyTexture(data->deviationTexture);
}

SDL_Surface* equalizeHistogram(SDL_Surface* graySurface) {

    if (!graySurface) { SDL_Log("Não há superfície da imagem para equalização!"); return NULL; }

    int intensityFrequency[256] = {0};
    obtainIntesityFrequency(graySurface, intensityFrequency);

    int width = graySurface->w;
    int height = graySurface->h;
    int totalPixels = width * height;
    if (totalPixels == 0) return NULL;

    float pr[256], cdf[256];
    for (int i = 0; i < 256; i++) pr[i] = (float)intensityFrequency[i] / totalPixels;

    cdf[0] = pr[0];
    for (int i = 1; i < 256; i++) cdf[i] = cdf[i - 1] + pr[i];

    Uint8 mapping[256];
    for (int i = 0; i < 256; i++) mapping[i] = (Uint8)(255 * cdf[i] + 0.5f);

    SDL_Surface* newSurface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    if (!newSurface) { SDL_Log("Erro ao criar superfície equalizada: %s", SDL_GetError()); return NULL; }

    if (!SDL_LockSurface(graySurface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície em escala de cinza",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        return NULL;
    }

    if (!SDL_LockSurface(newSurface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície de saída criada",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        SDL_UnlockSurface(graySurface);
        return NULL;
    }

    Uint8* srcBytes = (Uint8*)graySurface->pixels;
    Uint8* dstBytes = (Uint8*)newSurface->pixels;
    int srcPitchBytes = graySurface->pitch;
    int dstPitchBytes = newSurface->pitch;

    const SDL_PixelFormatDetails *srcFormat = SDL_GetPixelFormatDetails(graySurface->format);
    const SDL_PixelFormatDetails *dstFormat = SDL_GetPixelFormatDetails(newSurface->format);
    const SDL_Palette *srcPalette = SDL_GetSurfacePalette(graySurface);
    const SDL_Palette *dstPalette = SDL_GetSurfacePalette(newSurface);

    for (int y = 0; y < height; ++y) {
        Uint32* srcRow = (Uint32*)(srcBytes + y * srcPitchBytes);
        Uint32* dstRow = (Uint32*)(dstBytes + y * dstPitchBytes);
        for (int x = 0; x < width; ++x) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(srcRow[x], srcFormat, srcPalette, &r, &g, &b, &a);
            Uint8 newVal = mapping[r];
            dstRow[x] = SDL_MapRGBA(dstFormat, dstPalette, newVal, newVal, newVal, a);
        }
    }

    SDL_UnlockSurface(graySurface);
    SDL_UnlockSurface(newSurface);

    return newSurface;
}

SDL_Surface* applyColormap(SDL_Surface* graySurface, ColormapType cm) {
    if (!graySurface) return NULL;

    const SDL_PixelFormatDetails *srcFormat = SDL_GetPixelFormatDetails(graySurface->format);
    if (!srcFormat || srcFormat->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Log("applyColormap: esperado RGBA32. Converta antes com convertToRGBA/convertToGrey.");
    }

    SDL_Surface* colorSurf = SDL_CreateSurface(graySurface->w, graySurface->h, SDL_PIXELFORMAT_RGBA32);
    if (!colorSurf) { SDL_Log("applyColormap: falha ao criar surface destino: %s", SDL_GetError()); return NULL; }

    if (!SDL_LockSurface(graySurface)) { SDL_Log("applyColormap: SDL_LockSurface(src) falhou: %s", SDL_GetError()); SDL_DestroySurface(colorSurf); return NULL; }
    if (!SDL_LockSurface(colorSurf))   { SDL_Log("applyColormap: SDL_LockSurface(dst) falhou: %s", SDL_GetError()); SDL_UnlockSurface(graySurface); SDL_DestroySurface(colorSurf); return NULL; }

    Uint8* srcBytes = (Uint8*)graySurface->pixels;
    Uint8* dstBytes = (Uint8*)colorSurf->pixels;
    int srcPitchBytes = graySurface->pitch;
    int dstPitchBytes = colorSurf->pitch;

    const SDL_PixelFormatDetails *dstFormat = SDL_GetPixelFormatDetails(colorSurf->format);
    const SDL_Palette *srcPal = SDL_GetSurfacePalette(graySurface);
    const SDL_Palette *dstPal = SDL_GetSurfacePalette(colorSurf);

    for (int y = 0; y < graySurface->h; ++y) {
        Uint32* srcRow = (Uint32*)(srcBytes + y * srcPitchBytes);
        Uint32* dstRow = (Uint32*)(dstBytes + y * dstPitchBytes);
        for (int x = 0; x < graySurface->w; ++x) {
            Uint8 r,g,b,a;
            SDL_GetRGBA(srcRow[x], srcFormat, srcPal, &r, &g, &b, &a);
            uint8_t R,G,B;
            map_gray_to_rgb(r, cm, &R, &G, &B);
            dstRow[x] = SDL_MapRGBA(dstFormat, dstPal, R, G, B, a);
        }
    }

    SDL_UnlockSurface(colorSurf);
    SDL_UnlockSurface(graySurface);
    return colorSurf;
}

void drawButton(Button* btn, const char* label, TTF_Font* font) {

    if (btn->pressed) SDL_SetRenderDrawColor(renderer2, 0, 0, 128, 255);
    else if (btn->hovered) SDL_SetRenderDrawColor(renderer2, 100, 149, 237, 255);
    else SDL_SetRenderDrawColor(renderer2, 0, 0, 255, 255);

    SDL_RenderFillRect(renderer2, &btn->rect);

    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* textSurf = TTF_RenderText_Blended(font, label, 0, textColor);
    SDL_Texture* textTex = SDL_CreateTextureFromSurface(renderer2, textSurf);
    SDL_FRect textRect = {
        btn->rect.x + (btn->rect.w - textSurf->w) / 2,
        btn->rect.y + (btn->rect.h - textSurf->h) / 2,
        textSurf->w, textSurf->h
    };
    SDL_RenderTexture(renderer2, textTex, NULL, &textRect);
    SDL_DestroySurface(textSurf);
    SDL_DestroyTexture(textTex);
}

void shutdown(void) {
    SDL_Log("Shutdown...");
    SDL_Quit();
}

void setImagemTexture(SDL_Renderer* r,
                      SDL_Texture** pImagem,
                      ViewMode mode,
                      SDL_Surface* surfOrig,
                      SDL_Surface* surfEq,
                      SDL_Surface* surfColorOrig,
                      SDL_Surface* surfColorEq)
{
    if (*pImagem) SDL_DestroyTexture(*pImagem);
    SDL_Surface* chosen = NULL;
    switch (mode) {
        case VIEW_ORIGINAL:     chosen = surfOrig;      break;
        case VIEW_EQUALIZED:    chosen = surfEq;        break;
        case VIEW_PSEUDOCOLOR:  chosen = (surfColorEq ? surfColorEq : surfColorOrig); break;
    }
    *pImagem = SDL_CreateTextureFromSurface(r, chosen);
}

int main(int argc, char *argv[]){
    atexit(shutdown);

    const char* WINDOW_TITLE = "KISHIMOTO EFFECTS";
    const char* WINDOW_TITLE2 = "HISTOGRAMA";
    float largura;
    float altura;
    ImageData data;

    ViewMode viewMode = VIEW_ORIGINAL;
    ColormapType currentColormap = COLORMAP_JET;
    ViewMode lastGrayMode = VIEW_ORIGINAL;

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("Erro ao iniciar a SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Init TTF
    if (!TTF_Init()) {
        SDL_Log("Erro ao inicializar SDL_ttf: %s", SDL_GetError());
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
    SDL_Surface* surfaceEqualized = equalizeHistogram(surface);

    SDL_Surface* surfaceColorizedOriginal  = applyColormap(surface,          currentColormap);
    SDL_Surface* surfaceColorizedEqualized = applyColormap(surfaceEqualized, currentColormap);

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

    SDL_Texture* imagem = NULL;
    setImagemTexture(renderer, &imagem, viewMode,
                     surface, surfaceEqualized,
                     surfaceColorizedOriginal, surfaceColorizedEqualized);

    SDL_CreateWindowAndRenderer(WINDOW_TITLE2, 420, 540, 0,&window2, &renderer2);
    if (!window2) {
        SDL_Log("Erro ao criar a janela: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!renderer2) {
        SDL_Log("Erro ao criar o renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        return SDL_APP_FAILURE;
    }

    // Posicionar a segunda janela ao lado direito da primeira janela
    int window_x, window_y, window_w, window_h;
    int window2_x, window2_y;

    SDL_GetWindowPosition(window, &window_x, &window_y);
    SDL_GetWindowSize(window, &window_w, &window_h);

    window2_x = window_x + window_w;
    window2_y = window_y;

    SDL_SetWindowPosition(window2, window2_x, window2_y);

    TTF_Font* font = TTF_OpenFont("Roboto-Medium.ttf", 24);
    if (!font) { SDL_Log("Erro ao carregar a fonte: %s", SDL_GetError()); TTF_Quit(); return SDL_APP_FAILURE; }

    SDL_Color textColor = { 255, 255, 255, 255 };

    SDL_Surface* textLabelXSurface = TTF_RenderText_Blended(font, "Intensidade", 0, textColor);
    if (!textLabelXSurface) { SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError()); return SDL_APP_FAILURE; }

    SDL_Texture* textLabelXTexture = SDL_CreateTextureFromSurface(renderer2, textLabelXSurface);
    if (!textLabelXTexture) { SDL_Log("Erro ao criar a textura: %s", SDL_GetError()); return SDL_APP_FAILURE; }

    SDL_FRect textLabelXRect = { 0, 0, textLabelXSurface->w, textLabelXSurface->h };
    textLabelXRect.x = 150.0f;
    textLabelXRect.y = 300.0f;
    SDL_DestroySurface(textLabelXSurface);

    TTF_Font* fontRot = TTF_OpenFont("Roboto-Medium.ttf", 80);

    SDL_Surface* textLabelYSurface = TTF_RenderText_Blended(fontRot, "Frequência", 0, textColor);
    if (!textLabelYSurface) { SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError()); return SDL_APP_FAILURE; }

    SDL_Texture* textLabelYTexture = SDL_CreateTextureFromSurface(renderer2, textLabelYSurface);
    if (!textLabelYTexture) { SDL_Log("Erro ao criar a textura: %s", SDL_GetError()); return SDL_APP_FAILURE; }

    SDL_FRect textLabelYRect = { 0, 0, textLabelYSurface->w, textLabelYSurface->h };
    textLabelYRect.w = textLabelYSurface->w / 3.5f;
    textLabelYRect.h = textLabelYSurface->h / 3.5f;
    textLabelYRect.x = 0.0f;
    textLabelYRect.y = 160.0f;
    SDL_DestroySurface(textLabelYSurface);

    Button toggleBtn = {{ 20, 420, 180, 50}, false, false}; // Original <-> Equalizado
    Button colorBtn  = {{210, 420, 180, 50}, false, false}; // Pseudo ON/OFF
    Button cmapBtn   = {{ 20, 480, 370, 50}, false, false}; // Trocar colormap

    SDL_Event event;
    bool isRunning = true;

    bool renderizarJanelaPrincipal = true;
    bool renderizarJanelaSecundaria = true;

    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) {
                isRunning = false;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED){
                isRunning = false;
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = event.motion.x, my = event.motion.y;

                bool oldHover1 = toggleBtn.hovered;
                bool oldHover2 = colorBtn.hovered;
                bool oldHover3 = cmapBtn.hovered;

                toggleBtn.hovered = (mx >= toggleBtn.rect.x && mx <= toggleBtn.rect.x+toggleBtn.rect.w &&
                                     my >= toggleBtn.rect.y && my <= toggleBtn.rect.y+toggleBtn.rect.h);
                colorBtn.hovered  = (mx >= colorBtn.rect.x  && mx <= colorBtn.rect.x + colorBtn.rect.w &&
                                     my >= colorBtn.rect.y  && my <= colorBtn.rect.y + colorBtn.rect.h);
                cmapBtn.hovered   = (mx >= cmapBtn.rect.x   && mx <= cmapBtn.rect.x  + cmapBtn.rect.w &&
                                     my >= cmapBtn.rect.y   && my <= cmapBtn.rect.y  + cmapBtn.rect.h);

                if (oldHover1 != toggleBtn.hovered || oldHover2 != colorBtn.hovered || oldHover3 != cmapBtn.hovered){
                    renderizarJanelaSecundaria = true;
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                bool old1 = toggleBtn.pressed;
                bool old2 = colorBtn.pressed;
                bool old3 = cmapBtn.pressed;

                if (toggleBtn.hovered) toggleBtn.pressed = true;
                if (colorBtn.hovered)  colorBtn.pressed  = true;
                if (cmapBtn.hovered)   cmapBtn.pressed   = true;

                if (old1 != toggleBtn.pressed || old2 != colorBtn.pressed || old3 != cmapBtn.pressed){
                    renderizarJanelaSecundaria = true;
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (toggleBtn.pressed && toggleBtn.hovered) {
                    if (viewMode == VIEW_EQUALIZED) viewMode = VIEW_ORIGINAL;
                    else                            viewMode = VIEW_EQUALIZED;

                    setImagemTexture(renderer, &imagem, viewMode,
                                     surface, surfaceEqualized,
                                     surfaceColorizedOriginal, surfaceColorizedEqualized);
                    renderizarJanelaPrincipal = true;
                    renderizarJanelaSecundaria = true;
                }

                if (colorBtn.pressed && colorBtn.hovered) {
                    if (viewMode == VIEW_PSEUDOCOLOR) {
                        viewMode = lastGrayMode; // voltar ao último modo em cinza
                    } else {
                        lastGrayMode = viewMode;
                        viewMode = VIEW_PSEUDOCOLOR;
                    }

                    setImagemTexture(renderer, &imagem, viewMode,
                                     surface, surfaceEqualized,
                                     surfaceColorizedOriginal, surfaceColorizedEqualized);
                    renderizarJanelaPrincipal = true;
                    renderizarJanelaSecundaria = true;
                }

                if (cmapBtn.pressed && cmapBtn.hovered) {
                    currentColormap = (ColormapType)((currentColormap + 1) % COLORMAP_COUNT);

                    if (surfaceColorizedOriginal)  SDL_DestroySurface(surfaceColorizedOriginal);
                    if (surfaceColorizedEqualized) SDL_DestroySurface(surfaceColorizedEqualized);
                    surfaceColorizedOriginal  = applyColormap(surface,          currentColormap);
                    surfaceColorizedEqualized = applyColormap(surfaceEqualized, currentColormap);

                    if (viewMode == VIEW_PSEUDOCOLOR) {
                        setImagemTexture(renderer, &imagem, viewMode,
                                         surface, surfaceEqualized,
                                         surfaceColorizedOriginal, surfaceColorizedEqualized);
                        renderizarJanelaPrincipal = true;
                    }
                    renderizarJanelaSecundaria = true;
                }

                // reset press
                toggleBtn.pressed = false;
                colorBtn.pressed  = false;
                cmapBtn.pressed   = false;
            } else if (event.type == SDL_EVENT_KEY_UP){
                if (event.key.key == SDLK_S){
                    if (viewMode == VIEW_ORIGINAL){
                        if (!IMG_SavePNG(surface, "output_image.png")) {
                            SDL_Log("Erro ao salvar a imagem original: %s", SDL_GetError());
                            return SDL_APP_FAILURE;
                        } else {
                             SDL_Log("Imagem original salva com sucesso em output_image.png!");
                        }
                    } else if (viewMode == VIEW_EQUALIZED) {
                        if (!IMG_SavePNG(surfaceEqualized, "output_image.png")) {
                            SDL_Log("Erro ao salvar a imagem equalizada: %s", SDL_GetError());
                            return SDL_APP_FAILURE;
                        } else {
                            SDL_Log("Imagem equalizada salva com sucesso em output_image.png!");
                        }
                    } else { // VIEW_PSEUDOCOLOR
                        SDL_Surface* toSave = surfaceColorizedEqualized ? surfaceColorizedEqualized
                                                                        : surfaceColorizedOriginal;
                        if (!IMG_SavePNG(toSave, "output_image.png")) {
                            SDL_Log("Erro ao salvar a imagem pseudo-colorida: %s", SDL_GetError());
                            return SDL_APP_FAILURE;
                        } else {
                            SDL_Log("Imagem pseudo-colorida salva com sucesso em output_image.png!");
                        }
                    }
                } else if (event.key.key == SDLK_P) {
                    if (viewMode == VIEW_ORIGINAL)       viewMode = VIEW_EQUALIZED;
                    else if (viewMode == VIEW_EQUALIZED)  viewMode = VIEW_PSEUDOCOLOR;
                    else                                   viewMode = VIEW_ORIGINAL;

                    setImagemTexture(renderer, &imagem, viewMode,
                                     surface, surfaceEqualized,
                                     surfaceColorizedOriginal, surfaceColorizedEqualized);
                    renderizarJanelaPrincipal  = true;
                    renderizarJanelaSecundaria = true;
                } else if (event.key.key == SDLK_C) {
                    currentColormap = (ColormapType)((currentColormap + 1) % COLORMAP_COUNT);

                    if (surfaceColorizedOriginal)  SDL_DestroySurface(surfaceColorizedOriginal);
                    if (surfaceColorizedEqualized) SDL_DestroySurface(surfaceColorizedEqualized);

                    surfaceColorizedOriginal  = applyColormap(surface,          currentColormap);
                    surfaceColorizedEqualized = applyColormap(surfaceEqualized, currentColormap);

                    if (viewMode == VIEW_PSEUDOCOLOR) {
                        setImagemTexture(renderer, &imagem, viewMode,
                                         surface, surfaceEqualized,
                                         surfaceColorizedOriginal, surfaceColorizedEqualized);
                        renderizarJanelaPrincipal = true;
                    }
                    renderizarJanelaSecundaria = true;
                }
            }
        }

        if (renderizarJanelaPrincipal){
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_FRect dest = {0.0f, 0.0f, largura, altura};
            SDL_RenderTexture(renderer, imagem, NULL, &dest);

            SDL_RenderPresent(renderer);
            renderizarJanelaPrincipal = false;
        }

        if (renderizarJanelaSecundaria){
            SDL_SetRenderDrawColor(renderer2, 0, 0, 0, 255);
            SDL_RenderClear(renderer2);

            // Histograma/análise sempre em tons de cinza (original/equalizado)
            SDL_Surface* histSurf = (viewMode == VIEW_ORIGINAL) ? surface : surfaceEqualized;
            createHistogram(histSurf);
            analyzeImageInformation(histSurf, &data, font);

            SDL_RenderTexture(renderer2, data.intensityTexture, NULL, &data.intensityRect);
            SDL_RenderTexture(renderer2, data.deviationTexture, NULL, &data.deviationRect);
            SDL_RenderTexture(renderer2, textLabelXTexture, NULL, &textLabelXRect);
            SDL_RenderTextureRotated(renderer2, textLabelYTexture, NULL, &textLabelYRect, 270.0, NULL, SDL_FLIP_NONE);

            // Render dos botões
            drawButton(&toggleBtn, (viewMode == VIEW_EQUALIZED) ? "Equalizado" : "Original", font);
            drawButton(&colorBtn,  (viewMode == VIEW_PSEUDOCOLOR) ? "Pseudo: ON" : "Pseudo: OFF", font);

            char cmapLabel[64];
            snprintf(cmapLabel, sizeof(cmapLabel), "Colormap: %s (trocar)", colormapName(currentColormap));
            drawButton(&cmapBtn, cmapLabel, font);

            SDL_RenderPresent(renderer2);
            renderizarJanelaSecundaria = false;
        }
    }

    // destruir textures, renderer e janela principal
    SDL_DestroyTexture(imagem);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    window = NULL;
    renderer = NULL;

    SDL_DestroySurface(converted);
    SDL_DestroySurface(surface);
    SDL_DestroySurface(surfaceEqualized);

    SDL_DestroySurface(surfaceColorizedOriginal);
    SDL_DestroySurface(surfaceColorizedEqualized);

    SDL_DestroyTexture(textLabelXTexture);
    SDL_DestroyTexture(textLabelYTexture);
    destroyData(&data);
    SDL_DestroyRenderer(renderer2);
    SDL_DestroyWindow(window2);

    window2 = NULL;
    renderer2 = NULL;

    return 0;
}