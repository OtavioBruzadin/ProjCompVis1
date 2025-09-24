# O que é o projeto:

 Este projeto é uma aplicação gráfica desenvolvida em linguagem **C** utilizando a biblioteca **SDL3**, juntamente com suas extensões **SDL_image** e **SDL_ttf**, para manipulação de imagens e exibição de textos. O objetivo principal é permitir ao usuário realizar a equalização de imagens, analisar visualmente imagens em escala de cinza e calcular, além de exibir, o histograma de intensidade, a frequência dos pixels e o desvio padrão da imagem.

A aplicação cria duas janelas:
* **Janela principal** – fica do lado esquerdo e exibe a imagem carregada, seja na sua versão original ou após a equalização da imagem.
* **Janela secundária** – fica do lado direito e exibe o histograma da imagem, intensidade média e desvio padrão, além de um botão que permite o usuário alternar entre a imagem original e a versão equalizada.


# Como o projeto funciona:

O programa começa configurando a **SDL3**, criando a janela principal e associando um renderizador que será responsável por desenhar os elementos gráficos. Também são inicializados os módulos auxiliares **SDL_image** e **SDL_ttf**, que são necessários para trabalhar com as imagens e textos, respectivamente.

Foi criada uma struct para lidar com os dados da imagem, nela temos a intensidade e desvio padrão da textura.

Há uma verificação para o formato do arquivo enviado, que deve ser **png, jpg, jpeg, bmp, gif, tif, tiff ou webp**.

Em seguida, a imagem é carregada usando **IMG_Load**. Se ela não estiver em escala de cinza, ela será convertida para **RGBA** e para escala de cinza. Uma textura é criada a partir dessa superfície usando **SDL_CreateTextureFromSurface** para exibição pelo renderizador. As fontes são carregadas com **TTF_OpenFont**, permitindo a renderização da intensidade média e do desvio padrão.

Todas as vezes que houver uma manipulação da imagem, é necessário que a superfície seja bloqueada.

Para obtenção da intensidade, o programa inicia com um vetor de 256 posições, inicializado com zeros, que representará a frequência na escala de cinza de 0 a 255. Em seguida, ele percorre a imagem, conta a quantidade de pixels e cria um novo vetor para manipular os valores dos pixels. A cada pixel iterado, o programa incrementa o valor correspondente de intensidade no vetor de intensidade.

Baseado neste vetor, o histograma é criado, com o eixo **x** representando as escalas de cinza de 0 a 255 e o eixo **y** representando a maior frequência. Além disso, são criados textos com informações sobre a **média de intensidade** (que pode ser clara, média ou escura) e o **desvio padrão** (que pode ser baixo, médio ou alto).

Durante esses processos, o programa continuamente processa eventos do usuário. Assim, se alguma ação for tomada, o renderizador vai limpar a tela, desenhar as imagens e os textos atualizados ao usuário.

Além disso, se o usuário quiser salvar a imagem, é possível salvá-la em **PNG** segurando a tecla **“s”**.

Por fim, quando o usuário encerra a aplicação, todos os recursos são liberados da memória e o programa é finalizado.


# Contribuições para o projeto:

**Otávio:** Carregamento da imagem, verificação de formato e conversão para rgba, checagem para ver se está em escala de cinza e a conversão caso contrário e a criação da janela principal.

**Pedro:** Construção do histograma e análise das informações, com exibição dos resultados na janela secundária.

**Gabriel:** Equalização da imagem e seu botão correspondente, renderização eficiente e alterações estéticas na janela secundária.

**Guilherme:** Salvamento da imagem em PNG e documentação do README

# Para compilar: 
gcc main.c -I”caminho_para_o_include_do_SDL3” -o app -L”caminho_para_o_lib_do_SDL3” -lSDL3 -lSDL3_image -lSDL3_ttf

# Para rodar: 
./app “nome_do_arquivo_com_ext”
