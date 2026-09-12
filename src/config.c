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
    /* TODO: se d->n_erros >= MAX_DIAG, ignore silenciosamente (estourou o
     * limite). Senão, monte a mensagem formatada com va_start/vsnprintf
     * (vsnprintf(d->erros[d->n_erros], MAX_DIAG_TXT, fmt, args)) e
     * incremente d->n_erros. Não esquecer va_end. */
    (void)d;
    (void)fmt;
}

void diag_aviso(Diagnostico *d, const char *fmt, ...)
{
    /* TODO: igual a diag_erro, mas em d->avisos / d->n_avisos. */
    (void)d;
    (void)fmt;
}

/* ------------------------------ utilitários ------------------------------ */

int str_igual_ci(const char *a, const char *b)
{
    /* TODO: comparar caractere a caractere ignorando maiúsculas/minúsculas
     * (tolower((unsigned char)*a) == tolower((unsigned char)*b)), até
     * achar diferença ou os dois chegarem no '\0' ao mesmo tempo.
     * Cuidado com a===NULL/b==NULL antes de desreferenciar. */
    (void)a;
    (void)b;
    return 0;
}

/* Remove espaços/tabs/CR do início e do fim, IN PLACE.
 * Requisito 3.3.6: "espaços e linhas em branco não representam erros". */
static char *trim(char *s)
{
    /* TODO:
     * 1- apara o FIM primeiro: ande de trás pra frente sobrescrevendo
     *    espaços com '\0' enquanto isspace((unsigned char)s[len-1]).
     * 2- depois ache o INÍCIO: avance um ponteiro enquanto
     *    isspace((unsigned char)*inicio), e retorne esse ponteiro (não
     *    dá pra mover o conteúdo, só devolver onde o texto "de verdade"
     *    começa).
     *
     * ATENÇÃO (bug que eu mesmo caí ao testar): isso trima a LINHA
     * inteira, mas depois do split_campos cada CAMPO também pode ter
     * espaço sobrando ao redor (ex. "RM ; 3 ; 1" -> campo " 3 "). Chame
     * trim() de novo em cada campo depois de separar por ';', senão
     * "algoritmo ; 3 ; 1" quebra o campo_int por causa do espaço. */
    (void)s;
    return s;
}

static void str_maiuscula(char *s)
{
    /* TODO: percorrer a string e aplicar toupper((unsigned char)*s) em
     * cada posição, in place. */
    (void)s;
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
    /* TODO:
     * 1- usar strchr(ini, ';') repetidamente: cada vez que achar um ';',
     *    trocar por '\0', guardar 'ini' como um campo, e avançar 'ini'
     *    pra depois do ';'.
     * 2) quando strchr não achar mais ';', o resto da linha é o último
     *    campo (guardar e parar).
     * 3) requisito 3.3.3: se a linha terminava em ';', o passo 1 gera um
     *    último campo vazio "" à toa -- descartar esse campo vazio final
     *    (mas só o final; um campo vazio no MEIO, tipo "1;;5", é legítimo
     *    e deve ser preservado, conforme o comentário acima explica).
     * 4) respeitar o limite 'max' pra não estourar o array campos[]. */
    (void)linha;
    (void)campos;
    (void)max;
    return 0;
}

/* Lê um inteiro validando que o texto inteiro é numérico.
 * Campo vazio => devolve `padrao` (requisito 3.2).
 * Texto inválido => devolve 0 e sinaliza erro em *ok. */
static int campo_int(const char *txt, int padrao, int *ok)
{
    /* TODO: se txt for NULL/vazio, *ok=1 e devolve 'padrao'. Senão, usar
     * strtol(txt, &fim, 10) e conferir que 'fim' chegou no '\0' (ou seja,
     * a string inteira foi consumida, sem lixo depois do número) e que
     * fim != txt (realmente leu algo). Se não validar, *ok=0 e devolve 0. */
    *ok = 1;
    (void)txt;
    return padrao;
}

/* Converte "F0E0D0" em três componentes RGB.
 * Requisito 3.3: "cor é cor que identifica a execução da tarefa no formato
 * RGB em Hexadecimal, p.ex. 'F0E0D0' indica Red=F0, Green=E0, Blue=D0". */
static int campo_cor(const char *txt, TCB *t)
{
    /* TODO: campo vazio -> usar PADRAO_COR_R/G/B (requisito 3.2) e
     * devolver sucesso. Senão: aceitar um '#' opcional na frente, exigir
     * exatamente 6 dígitos hexadecimais depois, e usar sscanf(txt,
     * "%2x%2x%2x", &r, &g, &b) (com unsigned int) pra extrair os três
     * bytes. Validar cada caractere com isxdigit antes de confiar no
     * sscanf. Retornar 0 se o formato não bater. */
    (void)txt;
    (void)t;
    return 0;
}

/* ----------------------------- carga principal --------------------------- */

#define MAX_LINHA 1024

int config_carregar(const char *caminho, Estado *e, Diagnostico *d)
{
    /* TODO, nessa ordem:
     * 1- fopen(caminho, "r"); se falhar, diag_erro com o motivo e
     *    retornar 0 (não dá pra continuar sem arquivo).
     * 2) preencher *e com os valores PADRÃO (requisito 3.2): algoritmo =
     *    PADRAO_ALGORITMO, quantum = PADRAO_QUANTUM, ncpus = PADRAO_CPUS,
     *    tarefas/cpus = NULL, ntarefas = 0. Esses padrões são
     *    sobrescritos pelo que vier no arquivo, campo a campo.
     * 3) ler linha a linha com fgets(buf, MAX_LINHA, f), contando o
     *    número da linha (pras mensagens de erro).
     * 4) trim() a linha inteira; se ficou vazia, `continue` (3.3.6: linha
     *    em branco não é erro).
     * 5) split_campos() e, IMPORTANTE, trim() em CADA campo resultante
     *    (ver o comentário do bug em trim(), acima).
     * 6) primeira linha não-vazia = cabeçalho: campos[0]=algoritmo
     *    (aplicar str_maiuscula e copiar pra e->algoritmo com strncpy,
     *    respeitando o tamanho do array), campos[1]=quantum,
     *    campos[2]=qtde_cpus (validar >= 1). Usar campo_int e reportar
     *    erro claro se um campo não for número válido.
     * 7) linhas seguintes = tarefas: id, cor (campo_cor), ingresso,
     *    duracao, periodo, prazo (default = periodo, se vazio),
     *    lista_eventos (só guardar o texto bruto em t.eventos por
     *    enquanto, projeto B trata o resto).
     * 8) requisito 4.4: periodo == 0 -> diag_aviso (não é erro) e
     *    IGNORAR a tarefa (não entra no array); periodo < 0 -> diag_erro
     *    (isso sim é erro de arquivo).
     * 9) inicializar os campos de estado dinâmico da TCB (estado =
     *    EST_INATIVA, exec_restante=0, cpu=-1, ativacoes=0, etc.) antes
     *    de guardar no array.
     * 10) guardar cada tarefa válida em e->tarefas, crescendo o array
     *     dinamicamente (realloc dobrando a capacidade) -- não há limite
     *     de tarefas (requisito 3.3.1).
     * 11) depois do laço: fclose(f). Se nenhuma linha de cabeçalho foi
     *     lida (arquivo vazio), diag_erro.
     * 12) alocar e->cpus com e->ncpus entradas (id=i, tarefa=-1,
     *     ticks_desligada=0 cada uma).
     * 13) e->tick = 0.
     * 14) retornar 1 se d->n_erros == 0, senão 0 (mesmo que só ALGUMAS
     *     linhas tenham dado erro -- é melhor reportar todos os erros de
     *     uma vez do que parar no primeiro, por isso os `continue` em vez
     *     de `return` nos passos acima). */
    (void)caminho;
    (void)e;
    (void)d;
    return 0;
}