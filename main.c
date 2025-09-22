#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// cria windows e renderers
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

SDL_Window *window2 = NULL;
SDL_Renderer *renderer2 = NULL;

// cria tipo de variável para armanzenar informações da imagem
typedef struct ImageData ImageData;
struct ImageData{
    SDL_Texture* intensityTexture;
    SDL_FRect    intensityRect;
    SDL_Texture* deviationTexture;
    SDL_FRect    deviationRect;
};

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
    SDL_Log("Conversão para RGBA concluida");

    return converted;
}

static bool isGreyScale(SDL_Surface* surface) {
    if (!surface) return false;

    // bloquear para manipulação
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
        return surface; // NÃO retorna NULL — mantém a superfície para uso posterior
    }

    if (!SDL_LockSurface(surface)) { // Precisa bloquear para podermos manipular
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
    
    // inicia vetor de frequência com zeros
    for (int i = 0; i < 256; ++i) {
        intensityFrequency[i] = 0;
    }

    // verifica quantidade de pixels, formato e cria vetor de pixels da superfície passada
    const size_t pixelCount = surface->w * surface->h;
    const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);

    Uint32 *pixels = (Uint32 *)surface->pixels;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    
    // percorre o vetor de pixels e incrementa o valor correspondente
    // no vetor de frequência de intensidade 
    for (size_t i = 0; i < pixelCount; ++i)
    {
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

        if (!(r == g && r == b && g == b)){
            SDL_Log("Imagem nao esta em escala de cinza");
            return;
        } else {
            intensityFrequency[r]++;
        }
    }
}

void createHistogram(SDL_Surface* surface) {

    if (!surface) {
        SDL_Log("Erro! Superficie Invalida.");
        return;
    }

    if (!SDL_LockSurface(surface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        return;
    }

    int intensityFrequency[256];
    
    obtainIntesityFrequency(surface, intensityFrequency);

    // normaliza vetor de intensidades
    float normalizedIntensity[256];

    for (int i = 0; i < 256; ++i) {
        normalizedIntensity[i] = 0.0f;
    }

    const size_t pixelCount = surface->w * surface->h;
    for (int i = 0; i < 256; i++)
    {
        normalizedIntensity[i] = (float)intensityFrequency[i]/pixelCount;
    }

    // cria retângulos para formar o histograma
    SDL_SetRenderDrawColor(renderer2, 0, 0, 0, 255);

    SDL_FRect rect = { .x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 0.0f };

    rect.x = 10.0f;
    rect.y = 10.0f;
    rect.w = 500 - 20.0f;
    rect.h = 500 - 20.0f;

    SDL_RenderRect(renderer2, &rect);

    rect.x = 80.0f;
    rect.y = 40.0f;
    rect.w = 257.0f;
    rect.h = 257.0f;

    SDL_RenderRect(renderer2, &rect);

    SDL_SetRenderDrawColor(renderer2, 50, 50, 50, 255);

    // desenha as linhas de frequência para cada intensidade da escala de cinza
    // obs: leva em conta a maior frequência como limite máximo
    float maxFrequency = 0.0f;
    int maxFrequencyIntensity;
    for (int i = 0; i <= 255; i++){
        if (maxFrequency < normalizedIntensity[i]) {
            maxFrequency = normalizedIntensity[i];
        }
    }

    for (int i = 0; i <= 255; i++){
        SDL_RenderLine(renderer2, (80.0f + (i+1)), (40.0f + 257.0f), (80.0f + (i+1)), ((40.0f + 257.0f)-((normalizedIntensity[i]/(maxFrequency))*257.0f)));
    }

    SDL_UnlockSurface(surface);
}

char* obtainImageClass(SDL_Surface* surface, int* intensityFrequency) {

    int soma = 0;

    for (int i = 0; i <= 255; i++){
        soma = soma + i*intensityFrequency[i];
    }

    // calcula a média de intensidade de todos os pixels somados
    const size_t pixelCount = surface->w * surface->h;
    float media = (float)soma/pixelCount;

    // define a intesidade da imagem com os limites (85 e 170), dividindo em 3 intervalos
    if (media < 85.0f) {
        return "Escura";
    } else if (media > 170.0f) {
        return "Clara";
    } else {
        return "Média";
    }
}

char* obtainImageDeviation(SDL_Surface* surface, int* intensityFrequency) {

    int min = 300;
    int max = -1;

    // achar os valores dos pixels com maior e menor intensidade
    for (int i = 0; i <= 255; i++){
        if(intensityFrequency[i] > 0){
            if(i < min){
                min = i;
            }
            if(i > max){
                max = i;
            }
        }
    }

    // define o desvio padrão da imagem com os limites (85 e 170), dividindo em 3 intervalos
    int var = abs(max-min);
    if (var < 85) {
        return "Baixo";
    } else if (var > 170) {
        return "Alto";
    } else {
        return "Médio";
    }
}

void analyzeImageInformation(SDL_Surface* surface, ImageData* data, TTF_Font* font){

    int intensityFrequency[256];

    SDL_Color textColor = { 0, 0, 0, 255 };

    // obtém lista com a quantidade de pixels para cada escala de cinza
    obtainIntesityFrequency(surface, intensityFrequency);

    // obtém classificação da imagem - INTENSIDADE (clara || média || escura)
    char* imageClass = obtainImageClass(surface, intensityFrequency);
    char classe[35]; 

    // concatena as strings para indicar a média de intesidade com o dado calculado
    snprintf(classe, 35, "Média de Intensidade: %s", imageClass);

    SDL_Surface* textIntensitySurface = TTF_RenderText_Blended(font, classe, 0, textColor);
    if (!textIntensitySurface) {
        SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError());
        return;
    }

    data->intensityTexture = SDL_CreateTextureFromSurface(renderer2, textIntensitySurface);
    if (!data->intensityTexture) {
        SDL_Log("Erro ao criar a textura: %s", SDL_GetError());
        return;
    }

    data->intensityRect.w = textIntensitySurface->w;
    data->intensityRect.h = textIntensitySurface->h;

    // posiciona o texto na janela
    data->intensityRect.x = 40.0f;
    data->intensityRect.y = 340.0f;

    SDL_DestroySurface(textIntensitySurface);

    // obtém classificação da imagem - DESVIO PADRÃO (clara || média || escura)
    char* imageDeviation = obtainImageDeviation(surface, intensityFrequency);

    char deviation[30]; 

    // concatena as strings para indicar o desvio padrão com o dado calculado
    snprintf(deviation, 30, "Desvio Padrão: %s", imageDeviation);

    SDL_Surface* textDeviationSurface = TTF_RenderText_Blended(font, deviation, 0, textColor);
    if (!textDeviationSurface) {
        SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError());
        return;
    }

    data->deviationTexture = SDL_CreateTextureFromSurface(renderer2, textDeviationSurface);
    if (!data->deviationTexture) {
        SDL_Log("Erro ao criar a textura: %s", SDL_GetError());
        return;
    }

    data->deviationRect.w = textDeviationSurface->w;
    data->deviationRect.h = textDeviationSurface->h;

    // posiciona o texto na janela
    data->deviationRect.x = 40.0f;
    data->deviationRect.y = 380.0f;

    SDL_DestroySurface(textDeviationSurface);
}

void destroyData(ImageData* data){

    if (!data) {
        SDL_Log("Não há informações disponíveis da imagem!");
        return;
    }

    SDL_DestroyTexture(data->intensityTexture);
    SDL_DestroyTexture(data->deviationTexture);
}

void shutdown(void) {
    SDL_Log("Shutdown...");
    SDL_Quit();
}

int main(int argc, char *argv[]){
    atexit(shutdown);

    const char* WINDOW_TITLE = "KISHIMOTO EFFECTS";
    const char* WINDOW_TITLE2 = "HISTOGRAMA";
    int intensityFrequency[256];
    float largura;
    float altura;
    ImageData data;
    
    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("Erro ao iniciar a SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Init TTF
    if (TTF_Init() == -1) {
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

    if (!imagem) {
        SDL_Log("Erro ao criar textura da imagem '%s': %s", argv[1], SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return SDL_APP_FAILURE;
    }

    SDL_CreateWindowAndRenderer(WINDOW_TITLE2, 500, 500, 0,&window2, &renderer2);
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

    // carrega fonte padrão
    TTF_Font* font = TTF_OpenFont("Roboto-Medium.ttf", 24);
    if (!font) {
        SDL_Log("Erro ao carregar a fonte: %s", SDL_GetError());
        TTF_Quit();
        return SDL_APP_FAILURE;
    }

    SDL_Color textColor = { 0, 0, 0, 255 };

    // cria a legenda do eixo x do histograma
    SDL_Surface* textLabelXSurface = TTF_RenderText_Blended(font, "Intensidade", 0, textColor);
    if (!textLabelXSurface) {
        SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Texture* textLabelXTexture = SDL_CreateTextureFromSurface(renderer2, textLabelXSurface);
    if (!textLabelXTexture) {
        SDL_Log("Erro ao criar a textura: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_FRect textLabelXRect = { 0, 0, textLabelXSurface->w, textLabelXSurface->h };

    // posiciona a legenda (eixo x) na tela
    textLabelXRect.x = 150.0f;
    textLabelXRect.y = 300.0f;

    SDL_DestroySurface(textLabelXSurface);

    // carregar uma fonte maior para lidar com o problema de borrado ao girar a palavra
    TTF_Font* fontRot = TTF_OpenFont("Roboto-Medium.ttf", 80);

    // cria a legenda do eixo y do histograma
    SDL_Surface* textLabelYSurface = TTF_RenderText_Blended(fontRot, "Frequência", 0, textColor);
    if (!textLabelYSurface) {
        SDL_Log("Erro ao renderizar o texto: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Texture* textLabelYTexture = SDL_CreateTextureFromSurface(renderer2, textLabelYSurface);
    if (!textLabelYTexture) {
        SDL_Log("Erro ao criar a textura: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_FRect textLabelYRect = { 0, 0, textLabelYSurface->w, textLabelYSurface->h };

    // Reduz as dimensoes da superficie da imagem para caber no retangulo menor criado, 
    // assim a o texto de maior tamanho borrado sera convertido em um texto menor mais nitido
    textLabelYRect.w = textLabelYSurface->w / 3.5f;
    textLabelYRect.h = textLabelYSurface->h / 3.5f;

    // posiciona a legenda (eixo y) na tela
    textLabelYRect.x = 0.0f;
    textLabelYRect.y = 160.0f;

    SDL_DestroySurface(textLabelYSurface);

    SDL_Event event;
    bool isRunning = true;
    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                isRunning = false;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) // temporário (fechar uma janela encerra o programa)
            {
                isRunning = false;
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // renderizar imagem na primeira janela
        SDL_FRect dest = {0.0f, 0.0f, largura, altura};
        SDL_RenderTexture(renderer, imagem, NULL, &dest);

        SDL_RenderPresent(renderer);

        SDL_SetRenderDrawColor(renderer2, 255, 255, 255, 255);
        SDL_RenderClear(renderer2);

        // renderizar histograma e textos informativos na segunda janela
        createHistogram(surface);
        analyzeImageInformation(surface, &data, font);
        SDL_RenderTexture(renderer2, data.intensityTexture, NULL, &data.intensityRect);
        SDL_RenderTexture(renderer2, data.deviationTexture, NULL, &data.deviationRect);
        SDL_RenderTexture(renderer2, textLabelXTexture, NULL, &textLabelXRect);
        SDL_RenderTextureRotated(renderer2, textLabelYTexture, NULL, &textLabelYRect, 270.0, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(renderer2);
    }
    // destruir textures, renderer e window da janela principal
    SDL_DestroyTexture(imagem);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    window = NULL;
    renderer = NULL;

    // destruir surface (da imagem), textures, renderer e window da janela secundária
    SDL_DestroySurface(surface);
    destroyData(&data);
    SDL_DestroyRenderer(renderer2);
    SDL_DestroyWindow(window2);

    window2 = NULL;
    renderer2 = NULL;

    return 0;
}