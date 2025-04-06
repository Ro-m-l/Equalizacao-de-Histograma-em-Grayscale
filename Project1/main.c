//------------------------------------------------------------------------------
// Copyright 2025 Andre Kishimoto
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
//Código modificado a base do código do prof. Andre Kishimoto. O código a seguir suporta equalização e criação de histogramas para imagens em grayscale
//------------------------------------------------------------------------------
// Exemplo: 04-invert_image
// O programa carrega o arquivo de imagem indicado na constante IMAGE_FILENAME
// e exibe o conteúdo na janela ("kodim23.png" pertence ao Kodak Image Set).
// A tecla '1' aplica a transformação de intensidade (negativo da imagem).
// Caso a imagem seja maior do que WINDOW_WIDTHxWINDOW_HEIGHT, a janela é
// redimensionada logo após a imagem ser carregada.
//
// Observação:
// Em um projeto mais realista, o código abaixo provavelmente seria refatorado.
// Por exemplo, as funções loadRGBA32() e invertImage() seriam divididas em
// funções menores e com responsabilidades específicas, além de não dependerem
// de variáveis globais (vide princípios SOLID).
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

//------------------------------------------------------------------------------
// Constants and enums
//------------------------------------------------------------------------------
static const char* WINDOW_TITLE = "Histogram equalization";
static const char* IMAGE_FILENAME = "kodim23.png";

enum constants
{
    WINDOW_WIDTH = 640,
    WINDOW_HEIGHT = 480,
    ADDITIONAL_WIDTH = 258 // A largura adicional para mostar o histograma
};

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Surface* surface = NULL;
static SDL_Texture* texture = NULL;
static SDL_FRect textureRect = { .x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 0.0f };
static int k; //Quantidade de bits para cada byte no pixel da imagem - "imagem de (k) bits" - Cálculo: bits_per_pixel / bytes_per_pixel
static int M; //Número de linhas da imagem
static int N; //Número de colunas da imagem
static int currentHistogram[256] = { 0 };
static double L; //Níveis discretos de intensidade
static bool checkGrayScale(void);

//------------------------------------------------------------------------------
// Function declaration
//------------------------------------------------------------------------------

/**
 * Carrega a imagem indicada no parâmetro `filename` e a converte para o formato
 * RGBA32, eliminando dependência do formato original da imagem.
 */
static void loadRGBA32(const char* filename);

/**
 * Acessa cada pixel da imagem (surface) e inverte sua intensidade.
 * Altera a global surface e atualiza a global texture.
 *
 * Assumimos que os pixels da imagem estão no formato RGBA32 e que os níveis de
 * intensidade estão no intervalo [0-255].
 *
 * Na verdade, podemos desconsiderar o canal Alpha, já que ele não terá seu
 * valor invertido. Neste caso, substituímos `SDL_GetRGBA()` e `SDL_MapRGBA()`
 * por `SDL_GetRGB()` e `SDL_MapRGB()`, respectivamente.
 */

static void greyImageArithmeticAverage(void);
static void greyImageWeightedSum(void);
static void imageHistogram(void); //Cria o histograma
static void histogramEqualization(void); //Equaliza o histograma e chama a função showHistogram
static void histogramRenderer(void); //Renderiza as barras do gráfico do histograma

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static SDL_AppResult initialize(void)
{
    SDL_Log("Iniciando SDL...\n");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("*** Erro ao iniciar a SDL: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("Criando janela e renderizador...\n");
    if (!SDL_CreateWindowAndRenderer(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
        &window, &renderer))
    {
        SDL_Log("Erro ao criar a janela e/ou renderizador: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static void shutdown(void)
{
    SDL_Log("Destruindo textura...\n");
    SDL_DestroyTexture(texture);
    texture = NULL;

    SDL_Log("Destruindo superfície...\n");
    SDL_DestroySurface(surface);
    surface = NULL;

    SDL_Log("Destruindo renderizador...\n");
    SDL_DestroyRenderer(renderer);
    renderer = NULL;

    SDL_Log("Destruindo janela...\n");
    SDL_DestroyWindow(window);
    window = NULL;

    SDL_Log("Encerrando SDL...\n");
    SDL_Quit();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static void loop(void)
{
    SDL_Event event;
    bool isRunning = true;
    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                isRunning = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_1 && !event.key.repeat)
                {
                    greyImageArithmeticAverage();
                }
                else if (event.key.key == SDLK_2 && !event.key.repeat)
                {
                    greyImageWeightedSum();
                }
                else if (event.key.key == SDLK_3 && !event.key.repeat)
                {
                    imageHistogram();
                }
                else if (event.key.key == SDLK_4 && !event.key.repeat)
                {
                    histogramEqualization();
                }
                else if (event.key.key == SDLK_5 && !event.key.repeat)
                {
                    loadRGBA32(IMAGE_FILENAME);
                }
                break;
            }
        }
            //SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, &textureRect, &textureRect);
            histogramRenderer();
            SDL_RenderPresent(renderer); //apresenta o que foi desenhado
    }
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
static void histogramRenderer(void) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE); // Verde
    for (int i = 0; i < 256; i++)
    {
        SDL_RenderLine(renderer, (float)i + (int)textureRect.w + 1, WINDOW_HEIGHT, (float)i + (int)textureRect.w + 1, WINDOW_HEIGHT - ((float)currentHistogram[i] / 17)); //x0, y0, x1, y1 (y0 é no teto da tela) // do modo que está configurado, o y1 aumentado deixa a barra maior para cima (subtraindo de window_height)
        // O número 15 na divisão foi escolhido arbitráriamente para apresentar o gráfico dada a imagem teste numa escala melhor
    }
    SDL_SetRenderDrawColor(renderer, 125, 125, 125, SDL_ALPHA_OPAQUE); // Evitar problemas com o fundo ficando Ciano
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void loadRGBA32(const char* filename)
{
    SDL_Log("Carregando imagem '%s'...\n", filename);
    surface = IMG_Load(filename);
    if (!surface)
    {
        SDL_Log("*** Erro ao carregar a imagem: %s\n", SDL_GetError());
        return;
    }

    SDL_Log("Convertendo superfície para formato RGBA32...\n");
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!converted)
    {
        SDL_Log("*** Erro ao converter superfície para formato RGBA32: %s\n", SDL_GetError());
        surface = NULL;
        return;
    }

    surface = converted;

    SDL_Log("Criando textura a partir da superfície...\n");
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_Log("*** Erro ao criar textura: %s\n", SDL_GetError());
        return;
    }

    SDL_GetTextureSize(texture, &textureRect.w, &textureRect.h);
}
//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
static void greyImageArithmeticAverage(void)
{
    if (!surface)
    {
        SDL_Log("*** Erro em greyImageArithmeticAverage(): Imagem inválida!\n");
        return;
    }

    SDL_LockSurface(surface);

    const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface->format);
    const size_t pixelCount = surface->w * surface->h;

    Uint32* pixels = (Uint32*)surface->pixels;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    Uint8 average = 0;

    for (size_t i = 0; i < pixelCount; ++i)
    {
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

        average = (r + g + b) / 3;

        pixels[i] = SDL_MapRGBA(format, NULL, average, average, average, a);
    }

    SDL_UnlockSurface(surface);

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
}
//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
static void greyImageWeightedSum(void)
{
    if (!surface)
    {
        SDL_Log("*** Erro em greyImageWeightedSum(): Imagem inválida!\n");
        return;
    }

    SDL_LockSurface(surface);

    const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface->format);
    const size_t pixelCount = surface->w * surface->h;

    Uint32* pixels = (Uint32*)surface->pixels;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    Uint8 average = 0;

    for (size_t i = 0; i < pixelCount; ++i)
    {
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

        average = (Uint8)(r * 0.2126 + g * 0.7152 + b * 0.0722);

        pixels[i] = SDL_MapRGBA(format, NULL, average, average, average, a);
    }

    SDL_UnlockSurface(surface);

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
}
//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
static void imageHistogram(void)
{
    if (!surface)
    {
        SDL_Log("*** Erro em imageHistogram(): Imagem inválida!\n");
        return;
    }
    
    const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface->format);
    const size_t pixelCount = surface->w * surface->h;

    Uint32* pixels = (Uint32*)surface->pixels;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;

    if (!checkGrayScale()) { printf("A imagem deve ser cinza.\n"); return; } // Processo para imagem cinza, apenas um valor será usado pois são todos iguais, a escolha entre o valor r, g e b para isso é arbitrária

    int histogram[256] = { 0 }; // Criando um histograma com 256 valores (0 a 255) para cada valor que terá num histograma
        
    for (size_t i = 0; i < pixelCount; ++i)
    {
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);
        histogram[r] += 1;
    }
        
    SDL_UnlockSurface(surface);
    SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    for (int i = 0; i < 256; i++) {
        currentHistogram[i] = histogram[i]; //Atualiza o histograma que será mostrado em histogramRenderer();
    }

    SDL_RenderPresent(renderer); // Renderiza na tela
}
//------------------------------------------------------------------------------
static void histogramEqualization(void) {
    if (!surface)
    {
        SDL_Log("*** Erro em histogramEqualization(): Imagem inválida!\n");
        return;
    }

    const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface->format);
    const size_t pixelCount = surface->w * surface->h;

    Uint32* pixels = (Uint32*)surface->pixels;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;

    if (!checkGrayScale()) { printf("A imagem deve ser cinza.\n"); return; }

    imageHistogram(); //cria os valores de currenthistogram
    double histogramE[256] = { 0 };

    k = format->bits_per_pixel / format->bytes_per_pixel;
    L = pow(2, k);  // Cálculo dos níveis discretos de intensidade L = 2 ^ k

    int M = surface->h;
    int N = surface->w;

    int MxN = M * N;

    double maxValue = -1;

    for (int k = 0; k < 255; k++) { //0 a 255 intensidades
        int soma = 0;

        for (int j = 0; j <= k; j++) //somatória de todas as quantidades de intensidades
        {                            //de 0 até o valor K atual
            soma += currentHistogram[j];

        }
        histogramE[k] = (L - 1) * soma / (M * N); // T(rk) = (L - 1) * somatória de nj / (M * N);
        if (histogramE[k] > maxValue) { maxValue = histogramE[k]; }
    }

    //Normalização
    //achar o maior valor no histograma para normalizar os outros com base nele
    for (int i = 0; i < 256; ++i) {
        if (histogramE[i] > maxValue) {
            maxValue = histogramE[i];
        }
    }

    // Normalizar os valores do histograma para 0-255
    //valorExemplo/valorMax será uma porcentagem do valorMax
    //multiplicado por 255 entra na escala porporcional, com valorMax sendo 255
    for (int i = 0; i < 256; ++i) {
        histogramE[i] = (histogramE[i] * 255) / maxValue;
    }

    int newHistogram[256] = { 0 }; // Novo histograma já equalizado

    for (size_t i = 0; i < pixelCount; ++i) {
        // Obtém o valor RGBA de cada pixel
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

        // r == g == b para uma imagem em grayscale, então apenas 'r' é usado
        // Altera o valor 'r' com base no histograma
        // Mapeando r para o novo valor no histograma
        r = g = b = (int)histogramE[r];  // R ganha intensidade nova calculada no histograma
        // Como histogramE foi usado para o calculo das intensidades
        // usa-se o novo histograma para obter a distribuição
        // E atualiza o valor do pixel com a nova cor
        pixels[i] = SDL_MapRGBA(format, NULL, r, g, b, a);
        newHistogram[r]++; 
    }


    for (int i = 0; i < 256; ++i) {
        currentHistogram[i] = newHistogram[i];
    }
    SDL_UnlockSurface(surface);
    SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
}
//------------------------------------------------------------------------------
static bool checkGrayScale(void) {
    const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface->format);
    const size_t pixelCount = surface->w * surface->h;

    Uint32* pixels = (Uint32*)surface->pixels;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;

    bool isGrayScale = true;  // Assume que a imagem é em escala de cinza
    for (size_t i = 0; i < pixelCount; ++i)
    {
        SDL_GetRGBA(pixels[i], format, NULL, &r, &g, &b, &a);

        if (r != g && g != b)
        {
            return(false); // A imagem não é em escala de cinza. Não é necessário continuar verificando
        }
    }
    return (true);
}
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    atexit(shutdown);

    if (initialize() == SDL_APP_FAILURE)
        return SDL_APP_FAILURE;

    loadRGBA32(IMAGE_FILENAME);

    if (textureRect.w > WINDOW_WIDTH || textureRect.h > WINDOW_HEIGHT)
    {
        SDL_SetWindowSize(window, (int)textureRect.w, (int)textureRect.h);
        SDL_SyncWindow(window);
    }

    int newWindowWidth = (int)textureRect.w + ADDITIONAL_WIDTH;
    SDL_SetWindowSize(window, newWindowWidth, (int)textureRect.h); //Ajuste para o histograma
    printf("Inputs (botões do teclado):\n1 - Transformar imagem atual em grayscale por algoritmo de média aritmética\n2 - Transformar imagem atual em grayscale por algoritmo de média ponderada\n3 - Apresentar histograma da imagem atual\n4 - Equaliza a imagem atual (atualiza histograma)\n5 - Recarrega a imagem original.\n");
    loop();

    return 0;
}