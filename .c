/*
 * Trabalho Final - Sistema de Gerenciamento de Rotas (GPS Simplificado)
 *
 * Versão de terminal (sem interface gráfica).
 *
 * Conceitos aplicados:
 *   - Grafo: cidades são vértices e estradas são arestas.
 *   - Lista de adjacência para armazenar as estradas de cada cidade.
 *   - Algoritmo de Dijkstra (com fila de prioridade) para o menor caminho.
 *   - Manipulação de arquivos para salvar e carregar a rede de cidades.
 *
 * Como compilar:  gcc gps_terminal.c -o gps_terminal -Wall -Wextra
 * Como executar:  ./gps_terminal
 *
 * Formato do arquivo de dados (texto):
 *   C;NomeDaCidade
 *   E;CidadeDeOrigem;CidadeDeDestino;DistanciaEmQuilometros
 * (cada linha "E" representa uma estrada em um único sentido; uma estrada
 *  de mão dupla é gravada como duas linhas "E")
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAXIMO_DE_CIDADES 100
#define TAMANHO_MAXIMO_DO_NOME 50
#define TAMANHO_MAXIMO_DA_LINHA 256
#define NOME_DO_ARQUIVO_PADRAO "rotas.txt"
#define DISTANCIA_INFINITA DBL_MAX
#define SEM_CIDADE_ANTERIOR (-1)
#define ESTRADA_INEXISTENTE (-1.0)

#define CIDADE_COM_NOME_INVALIDO (-1)
#define CIDADE_JA_CADASTRADA (-2)
#define LIMITE_DE_CIDADES_ATINGIDO (-3)

/* =====================================================================
 *  ESTRUTURAS DE DADOS
 * ===================================================================== */

/* Um nó da lista encadeada de estradas que saem de uma cidade */
typedef struct Estrada {
    int indice_da_cidade_de_destino;
    double distancia_em_quilometros;
    struct Estrada *proxima_estrada;
} Estrada;

/* O grafo: lista de adjacência indexada pelo número da cidade */
typedef struct {
    char nomes_das_cidades[MAXIMO_DE_CIDADES][TAMANHO_MAXIMO_DO_NOME];
    Estrada *lista_de_adjacencia[MAXIMO_DE_CIDADES];
    int quantidade_de_cidades;
} RedeDeCidades;

/* Item e fila de prioridade (heap mínima) usados pelo algoritmo de Dijkstra */
typedef struct {
    double distancia_acumulada;
    int indice_da_cidade;
} ItemDaFilaDePrioridade;

typedef struct {
    ItemDaFilaDePrioridade *itens;
    int quantidade_de_itens;
    int capacidade_total;
} FilaDePrioridade;

/* =====================================================================
 *  FUNÇÕES AUXILIARES DE TEXTO E ENTRADA
 * ===================================================================== */

/* Mostra a mensagem e lê uma linha. Retorna 0 se a entrada acabou (EOF). */
static int ler_texto_do_teclado(const char *mensagem, char *texto_lido, int tamanho_do_texto) {
    printf("%s", mensagem);
    if (fgets(texto_lido, tamanho_do_texto, stdin) == NULL) {
        texto_lido[0] = '\0';
        return 0;
    }
    texto_lido[strcspn(texto_lido, "\r\n")] = '\0';
    return 1;
}

static void remover_espacos_das_pontas(char *texto) {
    char *inicio_do_texto = texto;
    while (*inicio_do_texto != '\0' && isspace((unsigned char)*inicio_do_texto)) {
        inicio_do_texto++;
    }
    memmove(texto, inicio_do_texto, strlen(inicio_do_texto) + 1);

    size_t comprimento_do_texto = strlen(texto);
    while (comprimento_do_texto > 0 && isspace((unsigned char)texto[comprimento_do_texto - 1])) {
        comprimento_do_texto--;
        texto[comprimento_do_texto] = '\0';
    }
}

/* Lê um número real positivo; aceita vírgula ou ponto como separador decimal.
 * Retorna 1 se o número for válido (maior que zero), senão 0. */
static int ler_numero_positivo_do_teclado(const char *mensagem, double *numero_lido) {
    char texto_digitado[64];
    ler_texto_do_teclado(mensagem, texto_digitado, sizeof texto_digitado);
    for (char *caractere = texto_digitado; *caractere != '\0'; caractere++) {
        if (*caractere == ',') {
            *caractere = '.';
        }
    }
    char *final_da_conversao;
    double valor_convertido = strtod(texto_digitado, &final_da_conversao);
    if (texto_digitado[0] == '\0' || *final_da_conversao != '\0' || valor_convertido <= 0.0) {
        return 0;
    }
    *numero_lido = valor_convertido;
    return 1;
}

static int textos_sao_iguais_ignorando_maiusculas(const char *primeiro_texto, const char *segundo_texto) {
    while (*primeiro_texto != '\0' && *segundo_texto != '\0') {
        if (tolower((unsigned char)*primeiro_texto) != tolower((unsigned char)*segundo_texto)) {
            return 0;
        }
        primeiro_texto++;
        segundo_texto++;
    }
    return *primeiro_texto == *segundo_texto;
}

/* =====================================================================
 *  OPERAÇÕES SOBRE O GRAFO
 * ===================================================================== */

static void inicializar_rede_de_cidades(RedeDeCidades *rede) {
    rede->quantidade_de_cidades = 0;
    for (int indice_da_cidade = 0; indice_da_cidade < MAXIMO_DE_CIDADES; indice_da_cidade++) {
        rede->lista_de_adjacencia[indice_da_cidade] = NULL;
    }
}

static void liberar_estradas_e_esvaziar_rede(RedeDeCidades *rede) {
    for (int indice_da_cidade = 0; indice_da_cidade < rede->quantidade_de_cidades; indice_da_cidade++) {
        Estrada *estrada_atual = rede->lista_de_adjacencia[indice_da_cidade];
        while (estrada_atual != NULL) {
            Estrada *estrada_a_liberar = estrada_atual;
            estrada_atual = estrada_atual->proxima_estrada;
            free(estrada_a_liberar);
        }
        rede->lista_de_adjacencia[indice_da_cidade] = NULL;
    }
    rede->quantidade_de_cidades = 0;
}

/* Retorna o índice da cidade com esse nome, ou -1 se não existir */
static int buscar_indice_da_cidade_pelo_nome(const RedeDeCidades *rede, const char *nome_da_cidade) {
    for (int indice_da_cidade = 0; indice_da_cidade < rede->quantidade_de_cidades; indice_da_cidade++) {
        if (textos_sao_iguais_ignorando_maiusculas(rede->nomes_das_cidades[indice_da_cidade], nome_da_cidade)) {
            return indice_da_cidade;
        }
    }
    return -1;
}

/* Aceita o nome da cidade ou o número mostrado na listagem (1, 2, 3...) */
static int localizar_cidade_digitada(const RedeDeCidades *rede, const char *texto_digitado) {
    int indice_encontrado = buscar_indice_da_cidade_pelo_nome(rede, texto_digitado);
    if (indice_encontrado != -1) {
        return indice_encontrado;
    }
    if (texto_digitado[0] == '\0') {
        return -1;
    }
    for (const char *caractere = texto_digitado; *caractere != '\0'; caractere++) {
        if (!isdigit((unsigned char)*caractere)) {
            return -1;
        }
    }
    int numero_da_cidade = atoi(texto_digitado);
    if (numero_da_cidade >= 1 && numero_da_cidade <= rede->quantidade_de_cidades) {
        return numero_da_cidade - 1;
    }
    return -1;
}

/* Retorna o índice da nova cidade ou um dos códigos negativos de erro */
static int cadastrar_cidade_na_rede(RedeDeCidades *rede, const char *nome_da_cidade) {
    if (nome_da_cidade[0] == '\0' || strchr(nome_da_cidade, ';') != NULL) {
        return CIDADE_COM_NOME_INVALIDO;
    }
    if (buscar_indice_da_cidade_pelo_nome(rede, nome_da_cidade) != -1) {
        return CIDADE_JA_CADASTRADA;
    }
    if (rede->quantidade_de_cidades >= MAXIMO_DE_CIDADES) {
        return LIMITE_DE_CIDADES_ATINGIDO;
    }
    int indice_da_nova_cidade = rede->quantidade_de_cidades;
    strncpy(rede->nomes_das_cidades[indice_da_nova_cidade], nome_da_cidade, TAMANHO_MAXIMO_DO_NOME - 1);
    rede->nomes_das_cidades[indice_da_nova_cidade][TAMANHO_MAXIMO_DO_NOME - 1] = '\0';
    rede->lista_de_adjacencia[indice_da_nova_cidade] = NULL;
    rede->quantidade_de_cidades++;
    return indice_da_nova_cidade;
}

/* Cria (ou atualiza) a estrada que vai da cidade de origem até a de destino */
static void cadastrar_estrada_em_um_sentido(RedeDeCidades *rede, int indice_da_cidade_de_origem,
                                            int indice_da_cidade_de_destino, double distancia_em_quilometros) {
    for (Estrada *estrada_atual = rede->lista_de_adjacencia[indice_da_cidade_de_origem];
         estrada_atual != NULL; estrada_atual = estrada_atual->proxima_estrada) {
        if (estrada_atual->indice_da_cidade_de_destino == indice_da_cidade_de_destino) {
            estrada_atual->distancia_em_quilometros = distancia_em_quilometros;
            return;
        }
    }
    Estrada *nova_estrada = malloc(sizeof(Estrada));
    if (nova_estrada == NULL) {
        fprintf(stderr, "Memória insuficiente!\n");
        exit(EXIT_FAILURE);
    }
    nova_estrada->indice_da_cidade_de_destino = indice_da_cidade_de_destino;
    nova_estrada->distancia_em_quilometros = distancia_em_quilometros;
    nova_estrada->proxima_estrada = rede->lista_de_adjacencia[indice_da_cidade_de_origem];
    rede->lista_de_adjacencia[indice_da_cidade_de_origem] = nova_estrada;
}

/* Distância da estrada de origem até destino, ou ESTRADA_INEXISTENTE */
static double obter_distancia_da_estrada(const RedeDeCidades *rede, int indice_da_cidade_de_origem,
                                         int indice_da_cidade_de_destino) {
    for (const Estrada *estrada_atual = rede->lista_de_adjacencia[indice_da_cidade_de_origem];
         estrada_atual != NULL; estrada_atual = estrada_atual->proxima_estrada) {
        if (estrada_atual->indice_da_cidade_de_destino == indice_da_cidade_de_destino) {
            return estrada_atual->distancia_em_quilometros;
        }
    }
    return ESTRADA_INEXISTENTE;
}

/* =====================================================================
 *  FILA DE PRIORIDADE (HEAP MÍNIMA)
 * ===================================================================== */

static void inicializar_fila_de_prioridade(FilaDePrioridade *fila, int capacidade_inicial) {
    fila->itens = malloc(sizeof(ItemDaFilaDePrioridade) * capacidade_inicial);
    if (fila->itens == NULL) {
        fprintf(stderr, "Memória insuficiente!\n");
        exit(EXIT_FAILURE);
    }
    fila->quantidade_de_itens = 0;
    fila->capacidade_total = capacidade_inicial;
}

static void inserir_na_fila_de_prioridade(FilaDePrioridade *fila, double distancia_acumulada,
                                          int indice_da_cidade) {
    if (fila->quantidade_de_itens == fila->capacidade_total) {
        fila->capacidade_total *= 2;
        fila->itens = realloc(fila->itens, sizeof(ItemDaFilaDePrioridade) * fila->capacidade_total);
        if (fila->itens == NULL) {
            fprintf(stderr, "Memória insuficiente!\n");
            exit(EXIT_FAILURE);
        }
    }
    int posicao_atual = fila->quantidade_de_itens;
    fila->quantidade_de_itens++;
    while (posicao_atual > 0) {
        int posicao_do_pai = (posicao_atual - 1) / 2;
        if (fila->itens[posicao_do_pai].distancia_acumulada <= distancia_acumulada) {
            break;
        }
        fila->itens[posicao_atual] = fila->itens[posicao_do_pai];
        posicao_atual = posicao_do_pai;
    }
    fila->itens[posicao_atual].distancia_acumulada = distancia_acumulada;
    fila->itens[posicao_atual].indice_da_cidade = indice_da_cidade;
}

static ItemDaFilaDePrioridade remover_menor_item_da_fila(FilaDePrioridade *fila) {
    ItemDaFilaDePrioridade item_de_menor_distancia = fila->itens[0];
    fila->quantidade_de_itens--;
    ItemDaFilaDePrioridade ultimo_item = fila->itens[fila->quantidade_de_itens];

    int posicao_atual = 0;
    while (1) {
        int posicao_do_filho = 2 * posicao_atual + 1;
        if (posicao_do_filho >= fila->quantidade_de_itens) {
            break;
        }
        if (posicao_do_filho + 1 < fila->quantidade_de_itens &&
            fila->itens[posicao_do_filho + 1].distancia_acumulada < fila->itens[posicao_do_filho].distancia_acumulada) {
            posicao_do_filho++;
        }
        if (fila->itens[posicao_do_filho].distancia_acumulada >= ultimo_item.distancia_acumulada) {
            break;
        }
        fila->itens[posicao_atual] = fila->itens[posicao_do_filho];
        posicao_atual = posicao_do_filho;
    }
    if (fila->quantidade_de_itens > 0) {
        fila->itens[posicao_atual] = ultimo_item;
    }
    return item_de_menor_distancia;
}

/* =====================================================================
 *  ALGORITMO DE DIJKSTRA
 * ===================================================================== */

/* Calcula o menor caminho a partir da cidade de origem.
 * Preenche distancias_minimas[] e cidade_anterior_no_caminho[] (usada para
 * reconstruir o trajeto). Retorna 1 se existe caminho até o destino. */
static int calcular_menor_caminho(const RedeDeCidades *rede, int indice_da_cidade_de_origem,
                                  int indice_da_cidade_de_destino, double *distancias_minimas,
                                  int *cidade_anterior_no_caminho) {
    for (int indice_da_cidade = 0; indice_da_cidade < rede->quantidade_de_cidades; indice_da_cidade++) {
        distancias_minimas[indice_da_cidade] = DISTANCIA_INFINITA;
        cidade_anterior_no_caminho[indice_da_cidade] = SEM_CIDADE_ANTERIOR;
    }
    distancias_minimas[indice_da_cidade_de_origem] = 0.0;

    FilaDePrioridade fila_de_cidades_a_visitar;
    inicializar_fila_de_prioridade(&fila_de_cidades_a_visitar, 64);
    inserir_na_fila_de_prioridade(&fila_de_cidades_a_visitar, 0.0, indice_da_cidade_de_origem);

    while (fila_de_cidades_a_visitar.quantidade_de_itens > 0) {
        ItemDaFilaDePrioridade item_atual = remover_menor_item_da_fila(&fila_de_cidades_a_visitar);
        int indice_da_cidade_atual = item_atual.indice_da_cidade;

        /* Entrada desatualizada: já existe um caminho melhor para esta cidade */
        if (item_atual.distancia_acumulada > distancias_minimas[indice_da_cidade_atual]) {
            continue;
        }
        if (indice_da_cidade_atual == indice_da_cidade_de_destino) {
            break;
        }
        for (const Estrada *estrada_atual = rede->lista_de_adjacencia[indice_da_cidade_atual];
             estrada_atual != NULL; estrada_atual = estrada_atual->proxima_estrada) {
            int indice_do_vizinho = estrada_atual->indice_da_cidade_de_destino;
            double nova_distancia = distancias_minimas[indice_da_cidade_atual] + estrada_atual->distancia_em_quilometros;
            if (nova_distancia < distancias_minimas[indice_do_vizinho]) {
                distancias_minimas[indice_do_vizinho] = nova_distancia;
                cidade_anterior_no_caminho[indice_do_vizinho] = indice_da_cidade_atual;
                inserir_na_fila_de_prioridade(&fila_de_cidades_a_visitar, nova_distancia, indice_do_vizinho);
            }
        }
    }
    free(fila_de_cidades_a_visitar.itens);
    return distancias_minimas[indice_da_cidade_de_destino] != DISTANCIA_INFINITA;
}

/* =====================================================================
 *  ARQUIVOS: SALVAR E CARREGAR
 * ===================================================================== */

/* Retorna 0 em caso de sucesso e -1 se não conseguiu abrir o arquivo */
static int salvar_rede_em_arquivo(const RedeDeCidades *rede, const char *nome_do_arquivo) {
    FILE *arquivo = fopen(nome_do_arquivo, "w");
    if (arquivo == NULL) {
        return -1;
    }
    for (int indice_da_cidade = 0; indice_da_cidade < rede->quantidade_de_cidades; indice_da_cidade++) {
        fprintf(arquivo, "C;%s\n", rede->nomes_das_cidades[indice_da_cidade]);
    }
    for (int indice_da_cidade = 0; indice_da_cidade < rede->quantidade_de_cidades; indice_da_cidade++) {
        for (const Estrada *estrada_atual = rede->lista_de_adjacencia[indice_da_cidade];
             estrada_atual != NULL; estrada_atual = estrada_atual->proxima_estrada) {
            fprintf(arquivo, "E;%s;%s;%.3f\n", rede->nomes_das_cidades[indice_da_cidade],
                    rede->nomes_das_cidades[estrada_atual->indice_da_cidade_de_destino],
                    estrada_atual->distancia_em_quilometros);
        }
    }
    fclose(arquivo);
    return 0;
}

/* Retorna a quantidade de cidades carregadas, ou -1 se não abriu o arquivo */
static int carregar_rede_de_arquivo(RedeDeCidades *rede, const char *nome_do_arquivo,
                                    int *quantidade_de_linhas_ignoradas) {
    FILE *arquivo = fopen(nome_do_arquivo, "r");
    if (arquivo == NULL) {
        return -1;
    }
    liberar_estradas_e_esvaziar_rede(rede);
    *quantidade_de_linhas_ignoradas = 0;

    char linha_lida[TAMANHO_MAXIMO_DA_LINHA];
    while (fgets(linha_lida, sizeof linha_lida, arquivo) != NULL) {
        linha_lida[strcspn(linha_lida, "\r\n")] = '\0';
        if (linha_lida[0] == '\0') {
            continue;
        }
        char *tipo_do_registro = strtok(linha_lida, ";");
        if (tipo_do_registro != NULL && strcmp(tipo_do_registro, "C") == 0) {
            char *nome_da_cidade = strtok(NULL, ";");
            if (nome_da_cidade == NULL || cadastrar_cidade_na_rede(rede, nome_da_cidade) < 0) {
                (*quantidade_de_linhas_ignoradas)++;
            }
        } else if (tipo_do_registro != NULL && strcmp(tipo_do_registro, "E") == 0) {
            char *nome_da_cidade_de_origem = strtok(NULL, ";");
            char *nome_da_cidade_de_destino = strtok(NULL, ";");
            char *texto_da_distancia = strtok(NULL, ";");
            int indice_da_cidade_de_origem =
                (nome_da_cidade_de_origem != NULL) ? buscar_indice_da_cidade_pelo_nome(rede, nome_da_cidade_de_origem) : -1;
            int indice_da_cidade_de_destino =
                (nome_da_cidade_de_destino != NULL) ? buscar_indice_da_cidade_pelo_nome(rede, nome_da_cidade_de_destino) : -1;
            if (indice_da_cidade_de_origem < 0 || indice_da_cidade_de_destino < 0 || texto_da_distancia == NULL) {
                (*quantidade_de_linhas_ignoradas)++;
                continue;
            }
            cadastrar_estrada_em_um_sentido(rede, indice_da_cidade_de_origem, indice_da_cidade_de_destino,
                                            atof(texto_da_distancia));
        } else {
            (*quantidade_de_linhas_ignoradas)++;
        }
    }
    fclose(arquivo);
    return rede->quantidade_de_cidades;
}

/* =====================================================================
 *  OPÇÕES DO MENU
 * ===================================================================== */

static void exibir_menu_principal(void) {
    printf("\n========== GPS SIMPLIFICADO ==========\n");
    printf("1. Cadastrar cidade\n");
    printf("2. Cadastrar estrada\n");
    printf("3. Listar cidades e estradas\n");
    printf("4. Calcular menor rota\n");
    printf("5. Salvar dados em arquivo\n");
    printf("6. Carregar dados de arquivo\n");
    printf("0. Sair\n");
    printf("======================================\n");
}

static void exibir_cidades_numeradas(const RedeDeCidades *rede) {
    for (int indice_da_cidade = 0; indice_da_cidade < rede->quantidade_de_cidades; indice_da_cidade++) {
        printf("  %2d. %s\n", indice_da_cidade + 1, rede->nomes_das_cidades[indice_da_cidade]);
    }
}

static void executar_cadastro_de_cidade(RedeDeCidades *rede) {
    char nome_digitado[TAMANHO_MAXIMO_DO_NOME];
    ler_texto_do_teclado("Nome da cidade: ", nome_digitado, sizeof nome_digitado);
    remover_espacos_das_pontas(nome_digitado);

    int resultado_do_cadastro = cadastrar_cidade_na_rede(rede, nome_digitado);
    if (resultado_do_cadastro == CIDADE_COM_NOME_INVALIDO) {
        printf("Nome inválido (não pode ser vazio nem conter ponto e vírgula).\n");
    } else if (resultado_do_cadastro == CIDADE_JA_CADASTRADA) {
        printf("Essa cidade já está cadastrada.\n");
    } else if (resultado_do_cadastro == LIMITE_DE_CIDADES_ATINGIDO) {
        printf("Limite de %d cidades atingido.\n", MAXIMO_DE_CIDADES);
    } else {
        printf("Cidade '%s' cadastrada com sucesso.\n", rede->nomes_das_cidades[resultado_do_cadastro]);
    }
}

static void executar_cadastro_de_estrada(RedeDeCidades *rede) {
    if (rede->quantidade_de_cidades < 2) {
        printf("Cadastre pelo menos duas cidades antes de criar uma estrada.\n");
        return;
    }
    printf("\nCidades disponíveis:\n");
    exibir_cidades_numeradas(rede);

    char texto_da_cidade_de_origem[TAMANHO_MAXIMO_DO_NOME];
    char texto_da_cidade_de_destino[TAMANHO_MAXIMO_DO_NOME];
    char resposta_sobre_mao_dupla[16];
    ler_texto_do_teclado("Cidade de origem (nome ou número): ", texto_da_cidade_de_origem, sizeof texto_da_cidade_de_origem);
    ler_texto_do_teclado("Cidade de destino (nome ou número): ", texto_da_cidade_de_destino, sizeof texto_da_cidade_de_destino);
    remover_espacos_das_pontas(texto_da_cidade_de_origem);
    remover_espacos_das_pontas(texto_da_cidade_de_destino);

    int indice_da_cidade_de_origem = localizar_cidade_digitada(rede, texto_da_cidade_de_origem);
    int indice_da_cidade_de_destino = localizar_cidade_digitada(rede, texto_da_cidade_de_destino);
    if (indice_da_cidade_de_origem < 0 || indice_da_cidade_de_destino < 0) {
        printf("Cidade não encontrada. Cadastre-a primeiro.\n");
        return;
    }
    if (indice_da_cidade_de_origem == indice_da_cidade_de_destino) {
        printf("A origem e o destino devem ser cidades diferentes.\n");
        return;
    }

    double distancia_em_quilometros;
    if (!ler_numero_positivo_do_teclado("Distância em quilômetros: ", &distancia_em_quilometros)) {
        printf("Distância inválida: informe um número maior que zero.\n");
        return;
    }
    ler_texto_do_teclado("A estrada é de mão dupla? (s/n): ", resposta_sobre_mao_dupla, sizeof resposta_sobre_mao_dupla);
    int estrada_de_mao_dupla = (tolower((unsigned char)resposta_sobre_mao_dupla[0]) != 'n');

    cadastrar_estrada_em_um_sentido(rede, indice_da_cidade_de_origem, indice_da_cidade_de_destino, distancia_em_quilometros);
    if (estrada_de_mao_dupla) {
        cadastrar_estrada_em_um_sentido(rede, indice_da_cidade_de_destino, indice_da_cidade_de_origem, distancia_em_quilometros);
    }
    printf("Estrada cadastrada: %s %s %s (%.1f km)\n", rede->nomes_das_cidades[indice_da_cidade_de_origem],
           estrada_de_mao_dupla ? "<->" : "->", rede->nomes_das_cidades[indice_da_cidade_de_destino],
           distancia_em_quilometros);
}

static void executar_listagem_da_rede(const RedeDeCidades *rede) {
    if (rede->quantidade_de_cidades == 0) {
        printf("Nenhuma cidade cadastrada.\n");
        return;
    }
    printf("\n--- Cidades (%d) ---\n", rede->quantidade_de_cidades);
    exibir_cidades_numeradas(rede);

    printf("\n--- Estradas ---\n");
    int quantidade_de_estradas_listadas = 0;
    for (int indice_da_primeira_cidade = 0; indice_da_primeira_cidade < rede->quantidade_de_cidades; indice_da_primeira_cidade++) {
        for (int indice_da_segunda_cidade = indice_da_primeira_cidade + 1;
             indice_da_segunda_cidade < rede->quantidade_de_cidades; indice_da_segunda_cidade++) {
            double distancia_de_ida = obter_distancia_da_estrada(rede, indice_da_primeira_cidade, indice_da_segunda_cidade);
            double distancia_de_volta = obter_distancia_da_estrada(rede, indice_da_segunda_cidade, indice_da_primeira_cidade);
            const char *nome_da_primeira_cidade = rede->nomes_das_cidades[indice_da_primeira_cidade];
            const char *nome_da_segunda_cidade = rede->nomes_das_cidades[indice_da_segunda_cidade];

            if (distancia_de_ida != ESTRADA_INEXISTENTE && distancia_de_volta == distancia_de_ida) {
                printf("  %s <-> %s : %.1f km\n", nome_da_primeira_cidade, nome_da_segunda_cidade, distancia_de_ida);
                quantidade_de_estradas_listadas++;
            } else {
                if (distancia_de_ida != ESTRADA_INEXISTENTE) {
                    printf("  %s -> %s : %.1f km\n", nome_da_primeira_cidade, nome_da_segunda_cidade, distancia_de_ida);
                    quantidade_de_estradas_listadas++;
                }
                if (distancia_de_volta != ESTRADA_INEXISTENTE) {
                    printf("  %s -> %s : %.1f km\n", nome_da_segunda_cidade, nome_da_primeira_cidade, distancia_de_volta);
                    quantidade_de_estradas_listadas++;
                }
            }
        }
    }
    if (quantidade_de_estradas_listadas == 0) {
        printf("  Nenhuma estrada cadastrada.\n");
    }
}

/* Mostra o trajeto em uma linha, o custo total e um desenho vertical do caminho */
static void exibir_caminho_encontrado(const RedeDeCidades *rede, const int *cidades_do_caminho,
                                      int quantidade_de_cidades_no_caminho, const double *distancias_minimas) {
    int indice_da_cidade_de_destino = cidades_do_caminho[quantidade_de_cidades_no_caminho - 1];

    printf("\nTrajeto: ");
    for (int posicao_no_caminho = 0; posicao_no_caminho < quantidade_de_cidades_no_caminho; posicao_no_caminho++) {
        printf("%s", rede->nomes_das_cidades[cidades_do_caminho[posicao_no_caminho]]);
        if (posicao_no_caminho < quantidade_de_cidades_no_caminho - 1) {
            printf(" -> ");
        }
    }
    printf("\nCusto total: %.1f km\n", distancias_minimas[indice_da_cidade_de_destino]);
    printf("Estradas percorridas: %d\n", quantidade_de_cidades_no_caminho - 1);

    printf("\nVisualização do caminho:\n\n");
    for (int posicao_no_caminho = 0; posicao_no_caminho < quantidade_de_cidades_no_caminho; posicao_no_caminho++) {
        int indice_da_cidade_atual = cidades_do_caminho[posicao_no_caminho];
        const char *marcador_da_cidade = "( )";
        if (posicao_no_caminho == 0) {
            marcador_da_cidade = "(O)";
        } else if (posicao_no_caminho == quantidade_de_cidades_no_caminho - 1) {
            marcador_da_cidade = "(D)";
        }
        printf("  %s %s", marcador_da_cidade, rede->nomes_das_cidades[indice_da_cidade_atual]);
        if (posicao_no_caminho > 0) {
            printf("   [acumulado: %.1f km]", distancias_minimas[indice_da_cidade_atual]);
        }
        printf("\n");

        if (posicao_no_caminho < quantidade_de_cidades_no_caminho - 1) {
            int indice_da_proxima_cidade = cidades_do_caminho[posicao_no_caminho + 1];
            double distancia_do_trecho = distancias_minimas[indice_da_proxima_cidade] - distancias_minimas[indice_da_cidade_atual];
            printf("   |\n   |  %.1f km\n   v\n", distancia_do_trecho);
        }
    }
    printf("\n  Legenda: (O) origem   (D) destino\n");
}

static void executar_calculo_de_rota(const RedeDeCidades *rede) {
    if (rede->quantidade_de_cidades < 2) {
        printf("Cadastre pelo menos duas cidades antes de calcular uma rota.\n");
        return;
    }
    printf("\nCidades disponíveis:\n");
    exibir_cidades_numeradas(rede);

    char texto_da_cidade_de_origem[TAMANHO_MAXIMO_DO_NOME];
    char texto_da_cidade_de_destino[TAMANHO_MAXIMO_DO_NOME];
    ler_texto_do_teclado("Cidade de origem (nome ou número): ", texto_da_cidade_de_origem, sizeof texto_da_cidade_de_origem);
    ler_texto_do_teclado("Cidade de destino (nome ou número): ", texto_da_cidade_de_destino, sizeof texto_da_cidade_de_destino);
    remover_espacos_das_pontas(texto_da_cidade_de_origem);
    remover_espacos_das_pontas(texto_da_cidade_de_destino);

    int indice_da_cidade_de_origem = localizar_cidade_digitada(rede, texto_da_cidade_de_origem);
    int indice_da_cidade_de_destino = localizar_cidade_digitada(rede, texto_da_cidade_de_destino);
    if (indice_da_cidade_de_origem < 0 || indice_da_cidade_de_destino < 0) {
        printf("Cidade não encontrada.\n");
        return;
    }
    if (indice_da_cidade_de_origem == indice_da_cidade_de_destino) {
        printf("A origem e o destino são a mesma cidade (custo 0.0 km).\n");
        return;
    }

    double distancias_minimas[MAXIMO_DE_CIDADES];
    int cidade_anterior_no_caminho[MAXIMO_DE_CIDADES];
    int existe_caminho = calcular_menor_caminho(rede, indice_da_cidade_de_origem, indice_da_cidade_de_destino,
                                                distancias_minimas, cidade_anterior_no_caminho);
    if (!existe_caminho) {
        printf("Não existe rota entre %s e %s.\n", rede->nomes_das_cidades[indice_da_cidade_de_origem],
               rede->nomes_das_cidades[indice_da_cidade_de_destino]);
        return;
    }

    /* Reconstrói o caminho do destino até a origem e depois inverte a ordem */
    int caminho_do_destino_para_a_origem[MAXIMO_DE_CIDADES];
    int quantidade_de_cidades_no_caminho = 0;
    for (int indice_da_cidade_atual = indice_da_cidade_de_destino; indice_da_cidade_atual != SEM_CIDADE_ANTERIOR;
         indice_da_cidade_atual = cidade_anterior_no_caminho[indice_da_cidade_atual]) {
        caminho_do_destino_para_a_origem[quantidade_de_cidades_no_caminho] = indice_da_cidade_atual;
        quantidade_de_cidades_no_caminho++;
    }
    int cidades_do_caminho[MAXIMO_DE_CIDADES];
    for (int posicao_no_caminho = 0; posicao_no_caminho < quantidade_de_cidades_no_caminho; posicao_no_caminho++) {
        cidades_do_caminho[posicao_no_caminho] =
            caminho_do_destino_para_a_origem[quantidade_de_cidades_no_caminho - 1 - posicao_no_caminho];
    }
    exibir_caminho_encontrado(rede, cidades_do_caminho, quantidade_de_cidades_no_caminho, distancias_minimas);
}

static void executar_salvamento_em_arquivo(const RedeDeCidades *rede) {
    char nome_do_arquivo[128];
    ler_texto_do_teclado("Nome do arquivo (Enter para '" NOME_DO_ARQUIVO_PADRAO "'): ", nome_do_arquivo,
                         sizeof nome_do_arquivo);
    remover_espacos_das_pontas(nome_do_arquivo);
    const char *arquivo_escolhido = (nome_do_arquivo[0] != '\0') ? nome_do_arquivo : NOME_DO_ARQUIVO_PADRAO;

    if (salvar_rede_em_arquivo(rede, arquivo_escolhido) == 0) {
        printf("Dados salvos em '%s'.\n", arquivo_escolhido);
    } else {
        printf("Erro: não foi possível gravar o arquivo '%s'.\n", arquivo_escolhido);
    }
}

static void executar_carregamento_de_arquivo(RedeDeCidades *rede) {
    char nome_do_arquivo[128];
    ler_texto_do_teclado("Nome do arquivo (Enter para '" NOME_DO_ARQUIVO_PADRAO "'): ", nome_do_arquivo,
                         sizeof nome_do_arquivo);
    remover_espacos_das_pontas(nome_do_arquivo);
    const char *arquivo_escolhido = (nome_do_arquivo[0] != '\0') ? nome_do_arquivo : NOME_DO_ARQUIVO_PADRAO;

    int quantidade_de_linhas_ignoradas = 0;
    int quantidade_de_cidades_carregadas = carregar_rede_de_arquivo(rede, arquivo_escolhido, &quantidade_de_linhas_ignoradas);
    if (quantidade_de_cidades_carregadas < 0) {
        printf("Erro: o arquivo '%s' não foi encontrado.\n", arquivo_escolhido);
        return;
    }
    printf("Dados carregados de '%s': %d cidades.", arquivo_escolhido, quantidade_de_cidades_carregadas);
    if (quantidade_de_linhas_ignoradas > 0) {
        printf(" (%d linha(s) inválida(s) ignorada(s))", quantidade_de_linhas_ignoradas);
    }
    printf("\n");
}

/* =====================================================================
 *  PROGRAMA PRINCIPAL
 * ===================================================================== */

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   /* para exibir acentos corretamente no Windows */
#endif

    RedeDeCidades rede_de_cidades;
    inicializar_rede_de_cidades(&rede_de_cidades);

    /* Se existir o arquivo padrão, carrega os dados automaticamente ao iniciar */
    FILE *arquivo_padrao = fopen(NOME_DO_ARQUIVO_PADRAO, "r");
    if (arquivo_padrao != NULL) {
        fclose(arquivo_padrao);
        int quantidade_de_linhas_ignoradas = 0;
        int quantidade_de_cidades_carregadas =
            carregar_rede_de_arquivo(&rede_de_cidades, NOME_DO_ARQUIVO_PADRAO, &quantidade_de_linhas_ignoradas);
        printf("Dados carregados automaticamente de '%s' (%d cidades).\n", NOME_DO_ARQUIVO_PADRAO,
               quantidade_de_cidades_carregadas);
    }

    int opcao_escolhida = -1;
    do {
        exibir_menu_principal();
        char texto_da_opcao[16];
        if (!ler_texto_do_teclado("Escolha uma opção: ", texto_da_opcao, sizeof texto_da_opcao)) {
            break;   /* fim da entrada de dados */
        }
        char *final_da_conversao;
        opcao_escolhida = (int)strtol(texto_da_opcao, &final_da_conversao, 10);
        if (texto_da_opcao[0] == '\0' || *final_da_conversao != '\0') {
            opcao_escolhida = -1;
        }

        switch (opcao_escolhida) {
            case 1: executar_cadastro_de_cidade(&rede_de_cidades); break;
            case 2: executar_cadastro_de_estrada(&rede_de_cidades); break;
            case 3: executar_listagem_da_rede(&rede_de_cidades); break;
            case 4: executar_calculo_de_rota(&rede_de_cidades); break;
            case 5: executar_salvamento_em_arquivo(&rede_de_cidades); break;
            case 6: executar_carregamento_de_arquivo(&rede_de_cidades); break;
            case 0: printf("Encerrando o programa. Até logo!\n"); break;
            default: printf("Opção inválida. Tente novamente.\n");
        }
    } while (opcao_escolhida != 0);

    liberar_estradas_e_esvaziar_rede(&rede_de_cidades);
    return 0;
}
