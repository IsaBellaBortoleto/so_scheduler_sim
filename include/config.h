#ifndef CONFIG_H
#define CONFIG_H

#include "tcb.h"

#define PADRAO_ALGORITMO "RM"
#define PADRAO_QUANTUM   2
#define PADRAO_CPUS      1
#define PADRAO_COR_R     0x3B
#define PADRAO_COR_G     0x82
#define PADRAO_COR_B     0xF6

#define MAX_DIAG      64
#define MAX_DIAG_TXT 200

typedef struct {
    char erros[MAX_DIAG][MAX_DIAG_TXT];   int n_erros;
    char avisos[MAX_DIAG][MAX_DIAG_TXT];  int n_avisos;
} Diagnostico;

void diag_erro (Diagnostico *d, const char *fmt, ...);
void diag_aviso(Diagnostico *d, const char *fmt, ...);

int config_carregar(const char *caminho, Estado *e, Diagnostico *d);

int str_igual_ci(const char *a, const char *b);

#endif