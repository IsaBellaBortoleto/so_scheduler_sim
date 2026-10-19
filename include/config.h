#ifndef CONFIG_H
#define CONFIG_H

#include "tcb.h"
/* ============================================================================
 * Requisito 3.2
 * O software deve sugerir valores padrão para todos os parâmetros.
 * ==========================================================================*/
/* 
 * Requisito 4
 * O algoritmo Rate Monotonic (RM) baseia-se numa prioridade fixa e estática
 * atribuída consoante o período da tarefa (menor período = maior prioridade).
*/
#define PADRAO_ALGORITMO "RM"
/* O sistema deve permitir a simulação em múltiplos processadores, 
 * sendo no mínimo 1. */
#define PADRAO_QUANTUM   2
#define PADRAO_CPUS      1
/* Periodo nao informado vale 0, que pelo enunciado e "aperiodica": a tarefa
 * e ignorada com aviso (4.4). Nao inventamos um periodo positivo: simular
 * uma tarefa com um periodo que ninguem escreveu enganaria o usuario. */
#define PADRAO_PERIODO   0
/* 
 * Requisito 2.1
 * Cores padrão para o preenchimento do Gráfico de Gantt exigido. */
#define PADRAO_COR_R     0x3B
#define PADRAO_COR_G     0x82
#define PADRAO_COR_B     0xF6
/* Limites para as mensagens de diagnóstico. */
#define MAX_DIAG      64
#define MAX_DIAG_TXT 200

/* ============================================================================
 * Requisitos 3.3.6 e 5
 * Qualquer erro na configuração ou no uso do simulador deve ser 
 * apresentado de forma clara para o usuário, justificando o motivo.
 * O C "empacota" estes erros nesta struct e o app.js exibe-os na tela.
 * ==========================================================================*/
typedef struct {
    char erros[MAX_DIAG][MAX_DIAG_TXT];   int n_erros;
    char avisos[MAX_DIAG][MAX_DIAG_TXT];  int n_avisos;
} Diagnostico;

void diag_erro (Diagnostico *d, const char *fmt, ...);
void diag_aviso(Diagnostico *d, const char *fmt, ...);


/* Interpreta o CONTEUDO de um arquivo de configuracao (texto terminado em
 * '\0'), nao um caminho.
 *
 * Por que conteudo e nao caminho: Teste com o arquivo no
 * pendrive, sem copiar para a maquina. A letra do pendrive (E:, F:...)
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

/* Converte 'txt' em int. Retorna 1 se o texto INTEIRO e um numero ("12"),
 * 0 se nao e ("abc", "", "12x") ou nao cabe num int. */
int ler_inteiro(const char *txt, int *saida);

#endif