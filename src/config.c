/* ============================================================================
 * config.c — Parser do arquivo de configuração (requisito 3.3)
 * ----------------------------------------------------------------------------
 * Formato esperado (texto simples):
 *
 *   linha 1 : algoritmo_escalonamento;quantum;qtde_cpus
 *   linha N : id;cor;ingresso;duracao;periodo;prazo;lista_eventos
 *
 * Regras exigidas pelo enunciado e onde elas estão tratadas neste arquivo:
 *   3.3.2 strings case-insensitive .......... str_igual_ci / str_maiuscula
 *   3.3.3 ';' final é opcional ............... split_campos ignora campo vazio final
 *   3.3.4 qualquer caminho de arquivo ........ o caminho é parâmetro, não constante
 *   3.3.6 espaços e linhas em branco não são erro ... trim + salto de linha vazia
 *   3.3.6 erros claros com o motivo .......... diag_erro("linha %d: ...")
 * ==========================================================================*/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ------------------------------ diagnósticos ----------------------------- */

void diag_erro(Diagnostico *d, const char *fmt, ...)
{

}

void diag_aviso(Diagnostico *d, const char *fmt, ...)
{

}

/* ------------------------------ utilitários ------------------------------ */

int str_igual_ci(const char *a, const char *b)
{

}

/* Remove espaços/tabs/CR do início e do fim, IN PLACE.
 * Requisito 3.3.6: "espaços e linhas em branco não representam erros". */
static char *trim(char *s)
{

}

static void str_maiuscula(char *s)
{
}

/* Quebra a linha em campos separados por ';'.
 *
 * Por que NÃO usar strtok? Porque strtok funde separadores consecutivos:
 * "1;;5" viraria dois campos em vez de três, e um campo vazio é
 * semanticamente diferente de um campo ausente (campo vazio => usar o valor
 * padrão do requisito 3.2). A divisão manual preserva os campos vazios.
 *
 * Requisito 3.3.3: se a linha terminar em ';', o campo vazio final é
 * descartado — as duas formas devem ser aceitas.
 *
 * Retorna a quantidade de campos escritos em `campos`.
 */
static int split_campos(char *linha, char *campos[], int max)
{

}

/* Lê um inteiro validando que o texto inteiro é numérico.
 * Campo vazio => devolve `padrao` (requisito 3.2).
 * Texto inválido => devolve 0 e sinaliza erro em *ok. */
static int campo_int(const char *txt, int padrao, int *ok)
{
}

/* Converte "F0E0D0" (ou "#F0E0D0") em três componentes RGB.
 * Requisito 3.3: "cor é cor que identifica a execução da tarefa no formato
 * RGB em Hexadecimal, p.ex. 'F0E0D0' indica Red=F0, Green=E0, Blue=D0". */
static int campo_cor(const char *txt, TCB *t)
{
}

/* ----------------------------- carga principal --------------------------- */

int config_carregar(const char *caminho, Estado *e, Diagnostico *d)
{

}
