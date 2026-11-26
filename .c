/*
 * Sistema de Gerenciamento de Rotas (GPS Simplificado)
 * Grafo com lista de adjacencia + Dijkstra com heap minima
 * Persistencia em arquivo texto.
 *
 * Compilar: gcc gps.c -o gps -Wall
 * Executar: ./gps
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#define MAX_CIDADES 100
#define TAM_NOME 50
#define ARQUIVO_PADRAO "rotas.txt"

/* ---------- Estruturas ---------- */
typedef struct Aresta {
    int destino;
    double custo;
    struct Aresta *prox;
} Aresta;

typedef struct {
    char nomes[MAX_CIDADES][TAM_NOME];
    Aresta *adj[MAX_CIDADES];
    int n;
} Grafo;

/* ---------- Utilitarios ---------- */
static void ler_linha(const char *msg, char *buf, int tam) {
    printf("%s", msg);
    if (!fgets(buf, tam, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static double ler_numero(const char *msg) {
    char buf[64];
    ler_linha(msg, buf, sizeof buf);
    return atof(buf);
}

static int igual_ignorando_caso(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* ---------- Grafo ---------- */
static void grafo_init(Grafo *g) {
    g->n = 0;
    for (int i = 0; i < MAX_CIDADES; i++) g->adj[i] = NULL;
}

static void grafo_limpar(Grafo *g) {
    for (int i = 0; i < g->n; i++) {
        Aresta *a = g->adj[i];
        while (a) { Aresta *t = a->prox; free(a); a = t; }
        g->adj[i] = NULL;
    }
    g->n = 0;
}

static int buscar_cidade(const Grafo *g, const char *nome) {
    for (int i = 0; i < g->n; i++)
        if (igual_ignorando_caso(g->nomes[i], nome)) return i;
    return -1;
}

static int adicionar_cidade(Grafo *g, const char *nome) {
    if (nome[0] == '\0') return -1;
    if (buscar_cidade(g, nome) != -1) return -2;      /* ja existe */
    if (g->n >= MAX_CIDADES) return -3;               /* cheio */
    strncpy(g->nomes[g->n], nome, TAM_NOME - 1);
    g->nomes[g->n][TAM_NOME - 1] = '\0';
    g->adj[g->n] = NULL;
    return g->n++;
}

/* Insere (ou atualiza) uma aresta direcionada u -> v */
static void adicionar_aresta(Grafo *g, int u, int v, double custo) {
    for (Aresta *a = g->adj[u]; a; a = a->prox) {
        if (a->destino == v) { a->custo = custo; return; }
    }
    Aresta *nova = malloc(sizeof(Aresta));
    if (!nova) { fprintf(stderr, "Sem memoria!\n"); exit(1); }
    nova->destino = v;
    nova->custo = custo;
    nova->prox = g->adj[u];
    g->adj[u] = nova;
}

/* ---------- Heap minima (fila de prioridade) ---------- */
typedef struct { double dist; int v; } NoHeap;
typedef struct { NoHeap *dados; int tam, cap; } Heap;

static void heap_init(Heap *h, int cap) {
    h->dados = malloc(sizeof(NoHeap) * cap);
    if (!h->dados) { fprintf(stderr, "Sem memoria!\n"); exit(1); }
    h->tam = 0; h->cap = cap;
}

static void heap_push(Heap *h, double dist, int v) {
    if (h->tam == h->cap) {
        h->cap *= 2;
        h->dados = realloc(h->dados, sizeof(NoHeap) * h->cap);
        if (!h->dados) { fprintf(stderr, "Sem memoria!\n"); exit(1); }
    }
    int i = h->tam++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->dados[p].dist <= dist) break;
        h->dados[i] = h->dados[p];
        i = p;
    }
    h->dados[i].dist = dist; h->dados[i].v = v;
}

static NoHeap heap_pop(Heap *h) {
    NoHeap topo = h->dados[0];
    NoHeap ult = h->dados[--h->tam];
    int i = 0;
    while (1) {
        int f = 2 * i + 1;
        if (f >= h->tam) break;
        if (f + 1 < h->tam && h->dados[f + 1].dist < h->dados[f].dist) f++;
        if (h->dados[f].dist >= ult.dist) break;
        h->dados[i] = h->dados[f];
        i = f;
    }
    if (h->tam > 0) h->dados[i] = ult;
    return topo;
}

/* ---------- Dijkstra ---------- */
/* Retorna 1 se existe caminho. Preenche dist[] e prev[]. */
static int dijkstra(const Grafo *g, int origem, int destino, double *dist, int *prev) {
    for (int i = 0; i < g->n; i++) { dist[i] = DBL_MAX; prev[i] = -1; }
    dist[origem] = 0;

    Heap h; heap_init(&h, 64);
    heap_push(&h, 0, origem);

    while (h.tam > 0) {
        NoHeap atual = heap_pop(&h);
        int u = atual.v;
        if (atual.dist > dist[u]) continue;   /* entrada obsoleta */
        if (u == destino) break;
        for (Aresta *a = g->adj[u]; a; a = a->prox) {
            double nd = dist[u] + a->custo;
            if (nd < dist[a->destino]) {
                dist[a->destino] = nd;
                prev[a->destino] = u;
                heap_push(&h, nd, a->destino);
            }
        }
    }
    free(h.dados);
    return dist[destino] != DBL_MAX;
}

/* ---------- Funcionalidades ---------- */
static void cadastrar_cidade(Grafo *g) {
    char nome[TAM_NOME];
    ler_linha("Nome da cidade: ", nome, sizeof nome);
    int r = adicionar_cidade(g, nome);
    if (r == -1) printf("Nome invalido.\n");
    else if (r == -2) printf("Cidade ja cadastrada.\n");
    else if (r == -3) printf("Limite de cidades atingido.\n");
    else printf("Cidade '%s' cadastrada.\n", g->nomes[r]);
}

static void cadastrar_estrada(Grafo *g) {
    char a[TAM_NOME], b[TAM_NOME], op[8];
    ler_linha("Cidade de origem: ", a, sizeof a);
    ler_linha("Cidade de destino: ", b, sizeof b);
    int u = buscar_cidade(g, a), v = buscar_cidade(g, b);
    if (u < 0 || v < 0) { printf("Cidade nao encontrada. Cadastre-a primeiro.\n"); return; }
    if (u == v) { printf("Origem e destino devem ser diferentes.\n"); return; }
    double custo = ler_numero("Distancia (km): ");
    if (custo <= 0) { printf("A distancia deve ser positiva.\n"); return; }
    ler_linha("Mao dupla? (s/n): ", op, sizeof op);

    adicionar_aresta(g, u, v, custo);
    if (tolower((unsigned char)op[0]) != 'n') adicionar_aresta(g, v, u, custo);
    printf("Estrada cadastrada: %s -> %s (%.1f km)%s\n", g->nomes[u], g->nomes[v], custo,
           tolower((unsigned char)op[0]) != 'n' ? " [ida e volta]" : " [mao unica]");
}

static void listar(const Grafo *g) {
    if (g->n == 0) { printf("Nenhuma cidade cadastrada.\n"); return; }
    printf("\n=== Cidades (%d) ===\n", g->n);
    for (int i = 0; i < g->n; i++) printf("%2d. %s\n", i + 1, g->nomes[i]);
    printf("\n=== Estradas ===\n");
    int total = 0;
    for (int i = 0; i < g->n; i++)
        for (Aresta *a = g->adj[i]; a; a = a->prox) {
            printf("%s -> %s : %.1f km\n", g->nomes[i], g->nomes[a->destino], a->custo);
            total++;
        }
    if (total == 0) printf("Nenhuma estrada cadastrada.\n");
}

static void calcular_rota(const Grafo *g) {
    char a[TAM_NOME], b[TAM_NOME];
    ler_linha("Cidade de origem: ", a, sizeof a);
    ler_linha("Cidade de destino: ", b, sizeof b);
    int o = buscar_cidade(g, a), d = buscar_cidade(g, b);
    if (o < 0 || d < 0) { printf("Cidade nao encontrada.\n"); return; }

    double dist[MAX_CIDADES];
    int prev[MAX_CIDADES];
    if (!dijkstra(g, o, d, dist, prev)) {
        printf("Nao existe rota entre %s e %s.\n", g->nomes[o], g->nomes[d]);
        return;
    }

    /* reconstroi o caminho de tras para frente */
    int caminho[MAX_CIDADES], k = 0;
    for (int v = d; v != -1; v = prev[v]) caminho[k++] = v;

    printf("\nMenor rota: ");
    for (int i = k - 1; i >= 0; i--) {
        printf("%s", g->nomes[caminho[i]]);
        if (i > 0) printf(" -> ");
    }
    printf("\nCusto total: %.1f km\n", dist[d]);

    printf("\nDetalhes do trajeto:\n");
    for (int i = k - 1; i > 0; i--) {
        int u = caminho[i], v = caminho[i - 1];
        printf("  %s -> %s : %.1f km (acumulado: %.1f km)\n",
               g->nomes[u], g->nomes[v], dist[v] - dist[u], dist[v]);
    }
}

/* Formato:  C;Nome   e   E;Origem;Destino;Custo  (aresta direcionada) */
static void salvar(const Grafo *g, const char *arq) {
    FILE *f = fopen(arq, "w");
    if (!f) { printf("Erro ao abrir '%s' para escrita.\n", arq); return; }
    for (int i = 0; i < g->n; i++) fprintf(f, "C;%s\n", g->nomes[i]);
    for (int i = 0; i < g->n; i++)
        for (Aresta *a = g->adj[i]; a; a = a->prox)
            fprintf(f, "E;%s;%s;%.3f\n", g->nomes[i], g->nomes[a->destino], a->custo);
    fclose(f);
    printf("Dados salvos em '%s'.\n", arq);
}

static void carregar(Grafo *g, const char *arq) {
    FILE *f = fopen(arq, "r");
    if (!f) { printf("Arquivo '%s' nao encontrado.\n", arq); return; }
    grafo_limpar(g);

    char linha[256];
    int linhas_ruins = 0;
    while (fgets(linha, sizeof linha, f)) {
        linha[strcspn(linha, "\r\n")] = '\0';
        if (linha[0] == '\0') continue;
        char *tipo = strtok(linha, ";");
        if (tipo && strcmp(tipo, "C") == 0) {
            char *nome = strtok(NULL, ";");
            if (!nome || adicionar_cidade(g, nome) < 0) linhas_ruins++;
        } else if (tipo && strcmp(tipo, "E") == 0) {
            char *o = strtok(NULL, ";");
            char *d = strtok(NULL, ";");
            char *c = strtok(NULL, ";");
            int u = o ? buscar_cidade(g, o) : -1;
            int v = d ? buscar_cidade(g, d) : -1;
            if (u < 0 || v < 0 || !c) { linhas_ruins++; continue; }
            adicionar_aresta(g, u, v, atof(c));
        } else {
            linhas_ruins++;
        }
    }
    fclose(f);
    printf("Carregado de '%s': %d cidades.", arq, g->n);
    if (linhas_ruins) printf(" (%d linha(s) ignorada(s))", linhas_ruins);
    printf("\n");
}

static void menu(void) {
    printf("\n===== GPS SIMPLIFICADO =====\n"
           "1. Cadastrar cidade\n"
           "2. Cadastrar estrada\n"
           "3. Listar cidades e estradas\n"
           "4. Calcular menor rota\n"
           "5. Salvar dados\n"
           "6. Carregar dados\n"
           "0. Sair\n");
}

int main(void) {
    Grafo g;
    grafo_init(&g);
    char op[16], arq[128];

    /* tenta carregar o arquivo padrao ao iniciar */
    FILE *teste = fopen(ARQUIVO_PADRAO, "r");
    if (teste) { fclose(teste); carregar(&g, ARQUIVO_PADRAO); }

    do {
        menu();
        ler_linha("Opcao: ", op, sizeof op);
        switch (atoi(op)) {
            case 1: cadastrar_cidade(&g); break;
            case 2: cadastrar_estrada(&g); break;
            case 3: listar(&g); break;
            case 4: calcular_rota(&g); break;
            case 5:
                ler_linha("Arquivo (Enter = " ARQUIVO_PADRAO "): ", arq, sizeof arq);
                salvar(&g, arq[0] ? arq : ARQUIVO_PADRAO);
                break;
            case 6:
                ler_linha("Arquivo (Enter = " ARQUIVO_PADRAO "): ", arq, sizeof arq);
                carregar(&g, arq[0] ? arq : ARQUIVO_PADRAO);
                break;
            case 0: printf("Ate logo!\n"); break;
            default: printf("Opcao invalida.\n");
        }
    } while (atoi(op) != 0 || op[0] != '0');

    grafo_limpar(&g);
    return 0;
}
