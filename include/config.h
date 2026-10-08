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

/* Interpreta o CONTEUDO de um arquivo de configuracao (texto terminado em
 * '\0'), nao um caminho.
 *
 * Por que conteudo e nao caminho: o professor testa com o arquivo no
 * pendrive dele, sem copiar para a maquina. A letra do pendrive (E:, F:...)
 * muda de computador para computador, entao a pagina usa o seletor de
 * arquivo do sistema e envia os bytes para o servidor (POST /api/carregar).
 * O C nunca abre caminho nenhum -- e o requisito 3.3.4 ("qualquer lugar do
 * sistema de arquivos, incluindo pendrive") fica atendido pelo proprio
 * seletor do Windows.
 *
 * O texto pode vir de outra maquina: aceitar quebras CRLF ou LF e um BOM
 * UTF-8 (bytes EF BB BF) antes da primeira linha.
 *
 * Retorna 1 se carregou sem erros, 0 caso contrario (erros e avisos em d). */
int config_carregar_texto(const char *conteudo, Estado *e, Diagnostico *d);

/* Compara duas strings ignorando maiusculas/minusculas (requisito 3.3.2:
 * "rm" == "RM"). Retorna 1 se iguais, 0 se diferentes -- o contrario do
 * strcmp, entao da para usar direto em if. NULL em qualquer um dos lados
 * retorna 0 (inclusive NULL com NULL).
 *
 * So letras ASCII perdem a caixa: acentuadas em UTF-8 sao comparadas byte a
 * byte ("EDF" == "edf", mas "É" != "é"). */
int str_igual_ci(const char *a, const char *b);

#endif