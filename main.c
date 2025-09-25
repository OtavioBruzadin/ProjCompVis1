#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// cria janelas e renderers
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

// tipo de dado (botão)
typedef struct {
    SDL_FRect rect;
    bool hovered;
    bool pressed;
} Button;

// variável para ter a noção de qual imagem está sendo mostrada na janela principal
bool showingEqualized = false;

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
    
    // bloquear superfície para manipulação dos pixels
    if (!SDL_LockSurface(surface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        return;
    }

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

    // desbloquear superfície
    SDL_UnlockSurface(surface);
}

void createHistogram(SDL_Surface* surface) {

    if (!surface) {
        SDL_Log("Erro! Superficie Invalida.");
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
    SDL_SetRenderDrawColor(renderer2, 255, 255, 255, 255);

    // desing da janela secundária
    SDL_FRect rect1 = { .x = 2.0f, .y = 2.0f, .w = (420.0f-4.0f), .h = (500.0f-4.0f)};
    SDL_RenderRect(renderer2, &rect1);

    SDL_FRect rect2 = { .x = 4.0f, .y = 4.0f, .w = (420.0f-8.0f), .h = (500.0f-8.0f)};
    SDL_RenderRect(renderer2, &rect2);

    SDL_FRect rect3 = { .x = 6.0f, .y = 6.0f, .w = (420.0f-12.0f), .h = (500.0f-12.0f)};
    SDL_RenderRect(renderer2, &rect3);

    SDL_FRect rect4 = { .x = 8.0f, .y = 8.0f, .w = (420.0f-16.0f), .h = (500.0f-16.0f)};
    SDL_RenderRect(renderer2, &rect4);

    SDL_FRect rect = { .x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 0.0f };

    rect.x = 10.0f;
    rect.y = 10.0f;
    rect.w = 420.0f - 20.0f;
    rect.h = 500.0f - 20.0f;

    SDL_RenderRect(renderer2, &rect);

    rect.x = 80.0f;
    rect.y = 40.0f;
    rect.w = 257.0f;
    rect.h = 257.0f;

    SDL_RenderRect(renderer2, &rect);

    SDL_SetRenderDrawColor(renderer2, 255, 255, 255, 255);

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
        if (intensityFrequency[i] != 0){ // se não existir pixel com essa intensidade (não irá desenhar nenhum ponto)
            SDL_RenderLine(renderer2, (80.0f + (i+1)), (41.0f + 255.0f), (80.0f + (i+1)), ((41.0f + 255.0f)-((normalizedIntensity[i]/(maxFrequency))*256.0f)));
        }
    }
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

    SDL_Color textColor = { 255, 255, 255, 255 };

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

SDL_Surface* equalizeHistogram(SDL_Surface* graySurface) {

    if (!graySurface) {
        SDL_Log("Não há superfície da imagem para equalização!");
        return NULL;
    }

    int intensityFrequency[256] = {0};
    obtainIntesityFrequency(graySurface, intensityFrequency);

    // calcula o número de pixels da imagem
    int width = graySurface->w;
    int height = graySurface->h;
    int totalPixels = width * height;
    if (totalPixels == 0) return NULL;

    // vetores pare armazenar probabilidade de ocorrência (pr) e distribuição acumulada (cdf)
    float pr[256];
    float cdf[256];

    // normalização do histograma
    for (int i = 0; i < 256; i++)
        pr[i] = (float)intensityFrequency[i] / totalPixels;

    // calcular cdf, distribuição acumulada dos níveis de intensidade
    cdf[0] = pr[0];
    for (int i = 1; i < 256; i++)
        cdf[i] = cdf[i - 1] + pr[i];

    // mapear a intensidade de cada nível da escala de cinza para uma intensidade da imagem de saída
    Uint8 mapping[256];
    for (int i = 0; i < 256; i++)
        // pega o valor acumulado das proabilidades de intensidades até a intensidade "i" e 
        // multiplica pelo número de níveis de intensidade da escala de cinza - 1
        // obs: a adição do valor 0.5f faz com que o arredondamento seja correto, visto que
        // ao transforma um float em um Uint8 o programa trunca o valor decimal
        mapping[i] = (Uint8)(255 * cdf[i] + 0.5f);
 
    SDL_Surface* newSurface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    if (!newSurface) {
        SDL_Log("Erro ao criar superfície equalizada: %s", SDL_GetError());
        return NULL;
    }

    // Travar superfícies para manipulação dos pixels
    // Tenta dar "lock" na superfície em escala de cinza
    if (!SDL_LockSurface(graySurface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície em escala de cinza",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        return NULL;
    }

    // Tenta dar "lock" na superfície de saída
    if (!SDL_LockSurface(newSurface)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Falha ao bloquear superfície de saída criada",
                                 SDL_GetError(),
                                 NULL);
        SDL_Log("SDL_LockSurface falhou: %s", SDL_GetError());
        SDL_UnlockSurface(graySurface); // Destrava primeira superfície lockada
        return NULL;
    }

    // ponteiros brutos e pitches (em pixels), que é o comprimento total de uma linha de pixels guardada na memória
    Uint8* srcBytes = (Uint8*)graySurface->pixels;
    Uint8* dstBytes = (Uint8*)newSurface->pixels;
    int srcPitchBytes = graySurface->pitch;
    int dstPitchBytes = newSurface->pitch;
    int srcPitchPixels = srcPitchBytes / 4;
    int dstPitchPixels = dstPitchBytes / 4;

    // formatos para leitura e escrita
    const SDL_PixelFormatDetails *srcFormat = SDL_GetPixelFormatDetails(graySurface->format);
    const SDL_Palette *srcPalette = SDL_GetSurfacePalette(graySurface);
    const SDL_PixelFormatDetails *dstFormat = SDL_GetPixelFormatDetails(newSurface->format);
    const SDL_Palette *dstPalette = SDL_GetSurfacePalette(newSurface);

    // iterar por linhas e colunas respeitando pitch
    for (int y = 0; y < height; ++y) {
        Uint32* srcRow = (Uint32*)(srcBytes + y * srcPitchBytes);
        Uint32* dstRow = (Uint32*)(dstBytes + y * dstPitchBytes);
        for (int x = 0; x < width; ++x) {
            Uint8 r, g, b, a;
            // mapeia os pixels de entrada para saída com base no vetor calculado anteriormente
            SDL_GetRGBA(srcRow[x], srcFormat, srcPalette, &r, &g, &b, &a);
            Uint8 newVal = mapping[r];
            dstRow[x] = SDL_MapRGBA(dstFormat, dstPalette, newVal, newVal, newVal, a);
        }
    }

    // destrava surfaces
    SDL_UnlockSurface(graySurface);
    SDL_UnlockSurface(newSurface);

    return newSurface;
}

void drawButton(Button* btn, const char* label, TTF_Font* font) {

    // definir um tipo de cor para cada caso (pressionado, "hover" e neutro) ao renderizar o botão na janela
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

    SDL_CreateWindowAndRenderer(WINDOW_TITLE2, 420, 500, 0,&window2, &renderer2);
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

    SDL_Color textColor = { 255, 255, 255, 255 };

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

    // Reduz as dimensões da imagem para caber no retângulo menor criado, 
    // assim a o texto de maior tamanho borrado será convertido em um texto menor mais nítido
    textLabelYRect.w = textLabelYSurface->w / 3.5f;
    textLabelYRect.h = textLabelYSurface->h / 3.5f;

    // posiciona a legenda (eixo y) na tela
    textLabelYRect.x = 0.0f;
    textLabelYRect.y = 160.0f;

    SDL_DestroySurface(textLabelYSurface);

    // definir tamanho do botão e estados de pressionado e "hover" como falsos
    Button toggleBtn = {{110, 420, 200, 50}, false, false};

    SDL_Event event;
    bool isRunning = true;

    // variáveis para controlar qual janela deve ser atualizada
    bool renderizarJanelaPrincipal = true;
    bool renderizarJanelaSecundaria = true;
    bool hover;
    bool pressed;
    bool showingEqualized = false;

    // cria texturas para mostrar a imagem na janela principal
    imagem = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Texture* imagemEqualizada = SDL_CreateTextureFromSurface(renderer, surfaceEqualized);

    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                isRunning = false;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED){ // fechar uma janela encerra o programa
                isRunning = false;
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = event.motion.x, my = event.motion.y;
                hover = toggleBtn.hovered;
                // verifica se o cursor está na posição (dentro) do botão
                toggleBtn.hovered = (mx >= toggleBtn.rect.x && mx <= toggleBtn.rect.x+toggleBtn.rect.w &&
                               my >= toggleBtn.rect.y && my <= toggleBtn.rect.y+toggleBtn.rect.h);
                // se o estado de "hover" do botão mudou, atualiza a janela secundária
                if (hover != toggleBtn.hovered){
                    renderizarJanelaSecundaria = true;
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                pressed = toggleBtn.pressed;
                // verifica se o cursor estiver no botão e se ele foi pressionado 
                if (toggleBtn.hovered) toggleBtn.pressed = true;
                // se o estado de pressionado do botão mudou, atualiza a janela secundária
                if (pressed != toggleBtn.pressed){
                    renderizarJanelaSecundaria = true;
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                // se o botão de click do mouse foi solto e o estado do botão é pressionado e "hovered"
                if (toggleBtn.pressed && toggleBtn.hovered) {
                    showingEqualized = !showingEqualized;
                    // manda renderizar ambas as janelas
                    renderizarJanelaPrincipal = true;
                    renderizarJanelaSecundaria = true;
                }
                toggleBtn.pressed = false;
            // foi utilizado KEY_UP para evitar salvar várias vezes ao manter "s" pressionado
            } else if (event.type == SDL_EVENT_KEY_UP){
                if (event.key.key == SDLK_S){
                    // salva a imagem da janela principal, verificando o valor de showingEqualized
                    if (showingEqualized == false){
                        if (!IMG_SavePNG(surface, "output_image.png")) {
                            SDL_Log("Erro ao salvar a imagem original: %s", SDL_GetError());
                            return SDL_APP_FAILURE;
                        } else {
                             SDL_Log("Imagem original salva com sucesso em output_image.png!");
                        }
                    } else {
                        if (!IMG_SavePNG(surfaceEqualized, "output_image.png")) {
                            SDL_Log("Erro ao salvar a imagem equalizada: %s", SDL_GetError());
                            return SDL_APP_FAILURE;
                        } else {
                            SDL_Log("Imagem equalizada salva com sucesso em output_image.png!");
                        }
                    }
                }
            }
        }
        if (renderizarJanelaPrincipal){
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            
            // renderizar imagem na janela principal com a imagem correta beseado na variável showingEqualized
            SDL_FRect dest = {0.0f, 0.0f, largura, altura};
            if (showingEqualized == false){
                SDL_RenderTexture(renderer, imagem, NULL, &dest);
            } else {
                SDL_RenderTexture(renderer, imagemEqualizada, NULL, &dest);
            }

            SDL_RenderPresent(renderer);
            renderizarJanelaPrincipal = false;
        }

        if (renderizarJanelaSecundaria){
            SDL_SetRenderDrawColor(renderer2, 0, 0, 0, 255);
            SDL_RenderClear(renderer2);

            // renderizar histograma e textos informativos na janela secundária
            // analisa a imagem mostrada na janela principal
            if (showingEqualized == false){
                createHistogram(surface);
                analyzeImageInformation(surface, &data, font);
            } else {
                createHistogram(surfaceEqualized);
                analyzeImageInformation(surfaceEqualized, &data, font);
            }
            SDL_RenderTexture(renderer2, data.intensityTexture, NULL, &data.intensityRect);
            SDL_RenderTexture(renderer2, data.deviationTexture, NULL, &data.deviationRect);
            SDL_RenderTexture(renderer2, textLabelXTexture, NULL, &textLabelXRect);
            SDL_RenderTextureRotated(renderer2, textLabelYTexture, NULL, &textLabelYRect, 270.0, NULL, SDL_FLIP_NONE);
            drawButton(&toggleBtn, showingEqualized ? "Equalizado" : "Original", font);
            SDL_RenderPresent(renderer2);
            renderizarJanelaSecundaria = false;
        }
    }
    // destruir textures, renderer e janela principal
    SDL_DestroyTexture(imagem);
    SDL_DestroyTexture(imagemEqualizada);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    window = NULL;
    renderer = NULL;

    // destruir surfaces (da imagem RGBA, em escala de cinza e equalizada), 
    // textures, renderer e janela secundária
    SDL_DestroySurface(converted);
    SDL_DestroySurface(surface);
    SDL_DestroySurface(surfaceEqualized);
    SDL_DestroyTexture(textLabelXTexture);
    SDL_DestroyTexture(textLabelYTexture);
    destroyData(&data);
    SDL_DestroyRenderer(renderer2);
    SDL_DestroyWindow(window2);

    window2 = NULL;
    renderer2 = NULL;

    return 0;
}