/*
 * Trabalho Prático 1 — LPII 2026.1
 * Problema P1: Multiplicação de matrizes n×n com pthreads
 *
 * Compilar:  cmake -B build && cmake --build build
 * Executar:  ./build/matrix_parallel [num_threads]   (padrão: NUM_THREADS)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/*  Configurações facilmente alteráveis                                */
/* ------------------------------------------------------------------ */
#define N            1200      /* dimensão das matrizes n×n            */
#define NUM_THREADS  8         /* padrão quando não passado por argv   */
#define TOLERANCIA   1e-6      /* tolerância para comparação de doubles */

/* ------------------------------------------------------------------ */
/*  Estrutura de argumentos passada para cada thread                   */
/* ------------------------------------------------------------------ */
typedef struct {
    const double *A;       /* matriz A (somente leitura)    */
    const double *B;       /* matriz B (somente leitura)    */
    double       *C;       /* matriz C (escrita na fatia)   */
    int           n;       /* dimensão                      */
    int           ini;     /* primeira linha desta thread   */
    int           fim;     /* uma além da última linha      */
} Arg;

/* ------------------------------------------------------------------ */
/*  Função executada por cada thread                                   */
/*  Calcula as linhas [ini, fim) de C = A × B                         */
/* ------------------------------------------------------------------ */
static void *worker(void *p)
{
    Arg *a = (Arg *)p;
    const double *A = a->A;
    const double *B = a->B;
    double       *C = a->C;
    int           n = a->n;

    for (int i = a->ini; i < a->fim; i++) {
        for (int j = 0; j < n; j++) {
            double soma = 0.0;
            for (int k = 0; k < n; k++)
                soma += A[i*n + k] * B[k*n + j];
            C[i*n + j] = soma;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Versão sequencial (fornecida pelo enunciado)                       */
/* ------------------------------------------------------------------ */
static void multiplicar_seq(const double *A, const double *B,
                             double *C, int n)
{
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double soma = 0.0;
            for (int k = 0; k < n; k++)
                soma += A[i*n + k] * B[k*n + j];
            C[i*n + j] = soma;
        }
}

/* ------------------------------------------------------------------ */
/*  Versão paralela                                                    */
/* ------------------------------------------------------------------ */
static void multiplicar_par(const double *A, const double *B,
                             double *C, int n, int num_threads)
{
    pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    Arg       *args    = malloc((size_t)num_threads * sizeof(Arg));

    /* distribui as linhas entre as threads (balanceamento uniforme) */
    int linhas_por_thread = n / num_threads;
    int resto             = n % num_threads;
    int inicio = 0;

    for (int t = 0; t < num_threads; t++) {
        /* threads com índice < resto recebem uma linha extra */
        int fatia = linhas_por_thread + (t < resto ? 1 : 0);

        args[t].A   = A;
        args[t].B   = B;
        args[t].C   = C;
        args[t].n   = n;
        args[t].ini = inicio;
        args[t].fim = inicio + fatia;
        inicio += fatia;

        pthread_create(&threads[t], NULL, worker, &args[t]);
    }

    /* aguarda todas as threads terminarem */
    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    free(threads);
    free(args);
}

/* ------------------------------------------------------------------ */
/*  Utilitários de medição e validação                                 */
/* ------------------------------------------------------------------ */

/* retorna tempo em segundos com precisão de nanossegundos */
static double agora(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* preenche a matriz com valores pseudo-aleatórios determinísticos */
static void preencher(double *M, int n, double semente)
{
    for (int i = 0; i < n * n; i++)
        M[i] = semente * (i + 1) * 0.001;
}

/* compara C_seq e C_par elemento a elemento com tolerância */
static int verificar(const double *C_seq, const double *C_par,
                     int n, double tol)
{
    for (int i = 0; i < n * n; i++) {
        if (fabs(C_seq[i] - C_par[i]) > tol) {
            printf("FALHA: diferença %.2e no índice %d "
                   "(seq=%.6f par=%.6f)\n",
                   fabs(C_seq[i] - C_par[i]), i,
                   C_seq[i], C_par[i]);
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Executa N_EXEC medições e devolve a média descartando a 1ª         */
/* ------------------------------------------------------------------ */
#define N_EXEC 6   /* 1 aquecimento + 5 medições */

static double medir_seq(const double *A, const double *B,
                        double *C, int n)
{
    double total = 0.0;
    for (int r = 0; r < N_EXEC; r++) {
        memset(C, 0, (size_t)(n * n) * sizeof(double));
        double t0 = agora();
        multiplicar_seq(A, B, C, n);
        double dt = agora() - t0;
        if (r > 0) total += dt;   /* descarta r=0 (aquecimento) */
        printf("  seq execução %d: %.4f s\n", r, dt);
    }
    return total / (N_EXEC - 1);
}

static double medir_par(const double *A, const double *B,
                        double *C, int n, int num_threads)
{
    double total = 0.0;
    for (int r = 0; r < N_EXEC; r++) {
        memset(C, 0, (size_t)(n * n) * sizeof(double));
        double t0 = agora();
        multiplicar_par(A, B, C, n, num_threads);
        double dt = agora() - t0;
        if (r > 0) total += dt;
        printf("  par[%2d] execução %d: %.4f s\n", num_threads, r, dt);
    }
    return total / (N_EXEC - 1);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    int num_threads = (argc > 1) ? atoi(argv[1]) : NUM_THREADS;
    if (num_threads < 1) num_threads = 1;

    printf("=== Multiplicação de Matrizes %d×%d ===\n", N, N);
    printf("Threads: %d\n\n", num_threads);

    /* --- alocação (fora do cronômetro) --- */
    size_t tamanho = (size_t)N * N * sizeof(double);
    double *A     = malloc(tamanho);
    double *B     = malloc(tamanho);
    double *C_seq = malloc(tamanho);
    double *C_par = malloc(tamanho);

    if (!A || !B || !C_seq || !C_par) {
        fprintf(stderr, "Erro: memória insuficiente para matrizes %d×%d.\n", N, N);
        return 1;
    }

    /* preenche A e B (determinístico, fora do cronômetro) */
    preencher(A, N, 1.0);
    preencher(B, N, 2.0);

    /* ---------------------------------------------------------- */
    /*  Q2 — Baseline sequencial                                  */
    /* ---------------------------------------------------------- */
    printf("--- Medindo versão SEQUENCIAL (%d execuções) ---\n", N_EXEC);
    double t_seq = medir_seq(A, B, C_seq, N);
    printf("T_seq médio (sem aquecimento): %.4f s\n\n", t_seq);

    /* ---------------------------------------------------------- */
    /*  Q3 — Versão paralela + verificação de corretude           */
    /* ---------------------------------------------------------- */
    printf("--- Medindo versão PARALELA com %d thread(s) ---\n", num_threads);
    double t_par = medir_par(A, B, C_par, N, num_threads);
    printf("T_par médio (sem aquecimento): %.4f s\n\n", t_par);

    /* verificação automática */
    printf("--- Verificação de corretude ---\n");
    if (verificar(C_seq, C_par, N, TOLERANCIA))
        printf("Resultado: OK (diferença máxima < %.0e)\n\n", TOLERANCIA);
    else
        printf("Resultado: FALHA\n\n");

    /* speedup */
    double speedup = t_seq / t_par;
    printf("--- Resultados (T=%d threads) ---\n", num_threads);
    printf("  T_seq  = %.4f s\n", t_seq);
    printf("  T_par  = %.4f s\n", t_par);
    printf("  Speedup= %.2fx\n\n", speedup);

    /* ---------------------------------------------------------- */
    /*  Q4 — Estudo de escalabilidade (varredura de threads)      */
    /* ---------------------------------------------------------- */
    printf("--- Q4: Varredura de escalabilidade ---\n");
    int pontos[] = {1, 2, 4, 8};
    int num_pontos = (int)(sizeof(pontos) / sizeof(pontos[0]));

    printf("%-10s %-12s %-12s %-12s\n",
           "Threads", "Tempo (s)", "Speedup", "Eficiência");
    printf("%-10s %-12s %-12s %-12s\n",
           "-------", "---------", "-------", "----------");

    double t_base = -1.0;
    for (int p = 0; p < num_pontos; p++) {
        int T = pontos[p];
        /* usa apenas 1 execução por ponto na varredura para economizar tempo */
        memset(C_par, 0, tamanho);
        double t0 = agora();
        multiplicar_par(A, B, C_par, N, T);
        double t = agora() - t0;

        if (p == 0) t_base = t;   /* T=1 é a referência da varredura */
        double sp  = t_base / t;
        double ef  = sp / T;
        printf("%-10d %-12.4f %-12.2f %-12.2f\n", T, t, sp, ef);
    }

    free(A); free(B); free(C_seq); free(C_par);
    return 0;
}