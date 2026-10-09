/* ============================================================================
 * config.c — leitura do arquivo de configuração (requisito 3.3)
 *
 * FORMATO
 *   1ª linha útil : algoritmo;quantum;qtde_cpus
 *   demais linhas : id;cor;ingresso;duracao;periodo;prazo;lista_eventos
 *
 * RESUMO
 *  - Recebe o TEXTO do arquivo, não um caminho. Quem escolhe o arquivo é o
 *    seletor da página, então funciona de qualquer lugar, inclusive do
 *    pendrive (req. 3.3.4). O C nunca abre arquivo.
 *  - Erro REPROVA o arquivo; aviso não. Todos os erros saem de uma vez, com
 *    o número da linha (req. 3.3.6).
 *  - Arquivo de outra máquina: aceita BOM e quebras do Windows (CRLF).
 *
 * ONDE CADA REGRA DO ENUNCIADO É TRATADA
 *   3.2   valores padrão ................. campo vazio em campo_int/campo_cor
 *   3.3.1 sem limite de tarefas .......... vetor que dobra (realloc)
 *   3.3.2 maiúsculas/minúsculas .......... str_igual_ci, str_maiuscula
 *   3.3.3 ';' final opcional ............. split_campos
 *   3.3.6 espaços e linhas em branco ..... trim
 *   4.4   tarefa aperiódica (período 0) .. ignorada com AVISO
 *
 *
 *   período vazio ...... padrão 0 = aperiódica; ignorada, com aviso próprio
 *   prazo vazio ou 0 ... vale o período (o 0 escrito gera aviso)
 *   quantum 0 .......... sem limite de quantum
 *   lista de eventos ... guardada inteira; só é interpretada no Projeto B
 * ==========================================================================*/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

/* ------------------------------ diagnósticos ----------------------------- */

/* Acrescenta uma mensagem de erro à lista, formatada como no printf:
 *     diag_erro(d, "linha %d: quantum invalido \"%s\"", n, txt);
 * A lista tem tamanho fixo: depois de MAX_DIAG erros os seguintes são
 * ignorados (o arquivo já está reprovado de qualquer forma, n_erros > 0). */
void diag_erro(Diagnostico *d, const char *fmt, ...)
{
    /* TODO: se d->n_erros >= MAX_DIAG, ignore silenciosamente (estourou o
     * limite). Senão, monte a mensagem formatada com va_start/vsnprintf
     * (vsnprintf(d->erros[d->n_erros], MAX_DIAG_TXT, fmt, args)) e
     * incremente d->n_erros. Não esquecer va_end. */
    if (d->n_erros >= MAX_DIAG)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(d->erros[d->n_erros], MAX_DIAG_TXT, fmt, args);
    va_end(args);
    d->n_erros++;
}

/* Igual a diag_erro, mas na lista de AVISOS. Aviso não reprova o arquivo: é
 * para o que o usuário precisa saber e a simulação consegue contornar (ex.
 * tarefa aperiódica ignorada, requisito 4.4). */
void diag_aviso(Diagnostico *d, const char *fmt, ...)
{
    /* TODO: igual a diag_erro, mas em d->avisos / d->n_avisos. */
    if (d->n_avisos >= MAX_DIAG)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(d->avisos[d->n_avisos], MAX_DIAG_TXT, fmt, args);
    va_end(args);

    d->n_avisos++;
}
/* ------------------------------ utilitários ------------------------------ */

/* Compara dois textos sem diferenciar maiúsculas: "rm" == "RM" (req. 3.3.2).
 * Devolve 1 se iguais, 0 se diferentes (o contrário do strcmp). */
int str_igual_ci(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }

    while (*a && *b)
    {
        /* O cast para unsigned char é obrigatório: tolower com char negativo
         * (byte de letra acentuada, onde char é signed) é comportamento
         * indefinido. */
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

/* Tira espaços, tabs e '\r' do começo e do fim (req. 3.3.6). Não copia nada:
 * corta o fim com '\0' e devolve um ponteiro para o começo de verdade.
 *
 * USE O RETORNO: linha = trim(linha). Texto só de espaços devolve "", e é
 * assim que uma linha em branco é pulada sem virar erro.
 *
 * O '\r' é o que sobra no fim das linhas de arquivos do Windows: tirá-lo
 * aqui é o que faz o parser aceitar CRLF.
 *
 * Aparar a linha não apara os campos: em " 1 ; 5 " sobram "1 " e " 5" depois
 * do split. Por isso o trim é chamado de novo em CADA campo. */
static char *trim(char *s)
{
    /* Fim primeiro: anda de trás para frente trocando cada espaço por '\0'.
     * O tam > 0 vem antes para nunca ler s[-1] numa string vazia. */
    int tam = strlen(s);

    while (tam > 0 && isspace((unsigned char)s[tam - 1]))
    {
        s[tam - 1] = '\0';
        tam--;
    }

    /* Início: avança o ponteiro até o primeiro caractere que não é espaço. */
    char *inicio = s;

    while (*inicio && isspace((unsigned char)*inicio))
    {
        inicio++;
    }

    return inicio;
}
/* Converte a string para maiúsculas, IN PLACE (requisito 3.3.2: "rm", "Rm" e
 * "RM" são o mesmo algoritmo). NULL é ignorado.
 * Só letras ASCII mudam; bytes de letras acentuadas em UTF-8 ficam como
 * estão. O cast para unsigned char é obrigatório pelo mesmo motivo do
 * str_igual_ci: toupper com char negativo é comportamento indefinido. */
static void str_maiuscula(char *s)
{
    if (s == NULL)
        return;

    while (*s)
    {
        *s = toupper((unsigned char)*s);
        s++;
    }
}

/* Quebra a linha nos ';'. Não copia nada: troca cada ';' por '\0' e guarda em
 * `campos` um ponteiro para o começo de cada pedaço. Devolve quantos achou.
 *
 * Por que NÃO strtok: ele junta separadores seguidos, e "1;;5" viraria dois
 * campos. O vazio do MEIO precisa existir: significa "use o valor padrão"
 * (req. 3.2). Se sumisse, os campos seguintes andariam uma casa.
 *
 * O vazio do FIM é descartado: é só o resto depois de um ';' final, que o
 * enunciado manda aceitar (req. 3.3.3). */
static int split_campos(char *linha, char *campos[], int max)
{
    int n = 0;

    /* 'linha' aponta sempre para o começo do próximo campo.
     * O n < max impede estourar campos[]. */
    while (n < max)
    {
        /* O ÚLTIMO campo permitido não é cortado: fica com o resto da linha
         * inteiro, com os ';' que tiver. */
        char *sep = (n == max - 1) ? NULL : strchr(linha, ';');

        if (sep == NULL)
        {
            /* Não há mais ';': o resto é o último campo. Se ele é vazio,
             * a linha terminava em ';' (ou era vazia) e o campo é descartado.
             * Um vazio no meio ("1;;5") é preservado. */
            if (*linha != '\0')
                campos[n++] = linha;

            break;
        }

        *sep = '\0';
        campos[n++] = linha;
        linha = sep + 1;
    }

    return n;
}

/* Converte o texto de um campo em número.
 * Campo vazio => devolve 'padrao', sem erro.
 * Texto ruim => devolve 0 e põe 0 em *ok. */
static int campo_int(const char *txt, int padrao, int *ok)
{
    char *fim;
    long valor;

    *ok = 1;

    if (txt == NULL || *txt == '\0')
        return padrao;

    errno = 0;
    valor = strtol(txt, &fim, 10);

    /* fim == txt: nenhum dígito foi lido.
     * *fim != '\0': sobrou texto depois do número.
     * ERANGE/limites: número grande demais. */
    if (fim == txt || *fim != '\0' ||
        errno == ERANGE || valor < INT_MIN || valor > INT_MAX)
    {
        *ok = 0;
        return 0;
    }

    return (int)valor;
}

/* Converte "F0E0D0" em três componentes RGB.
 * Requisito 3.3: "cor é cor que identifica a execução da tarefa no formato
 * RGB em Hexadecimal, p.ex. 'F0E0D0' indica Red=F0, Green=E0, Blue=D0". */
static int campo_cor(const char *txt, TCB *t)
{
    unsigned int r, g, b;

    /* Campo vazio: usa a cor padrão. */
    if (txt == NULL || *txt == '\0')
    {
        t->r = PADRAO_COR_R;
        t->g = PADRAO_COR_G;
        t->b = PADRAO_COR_B;
        return 1;
    }

    /* '#' na frente é opcional. */
    if (*txt == '#')
        txt++;

    /* A cor deve ter exatamente 6 dígitos hexadecimais. */
    if (strlen(txt) != 6)
        return 0;

    for (int i = 0; i < 6; i++)
    {
        if (!isxdigit((unsigned char)txt[i]))
            return 0;
    }

    /* Exemplo: F0E0D0 -> F0, E0, D0. */
    sscanf(txt, "%2x%2x%2x", &r, &g, &b);

    t->r = (unsigned char)r;
    t->g = (unsigned char)g;
    t->b = (unsigned char)b;

    return 1;
}
/* ----------------------------- carga principal --------------------------- */

/* id;cor;ingresso;duracao;periodo;prazo;lista_eventos
 * O 7º campo é a lista de eventos e fica com TODO o resto da linha (ver
 * split_campos), então nenhum evento é perdido. */
#define MAX_CAMPOS 7

/* campo_int + mensagem de erro pronta: o valor tem que ser um inteiro
 * >= minimo. Em caso de erro registra o diagnóstico e zera *ok (nunca põe 1:
 * assim vários campos da mesma linha acumulam no mesmo *ok). */
static int campo_int_min(Diagnostico *d, int linha, const char *nome,
                         const char *txt, int padrao, int minimo, int *ok)
{
    int leu;
    int valor = campo_int(txt, padrao, &leu);

    if (!leu || valor < minimo)
    {
        diag_erro(d, "linha %d: %s invalido \"%s\" (esperado um inteiro >= %d)",
                  linha, nome, txt, minimo);
        *ok = 0;
    }

    return valor;
}

/* Parseia o texto de configuracao linha a linha: 1a linha valida = cabecalho
 * (algoritmo;quantum;qtde_cpus), demais = tarefas. Preenche *e e aloca
 * e->tarefas/e->cpus; em erro fatal libera tudo e retorna 0. */
int config_carregar_texto(const char *conteudo, Estado *e, Diagnostico *d)
{
    char vazio[1] = "";
    char *copia, *linha, *prox;
    int num = 0;
    int tem_cabecalho = 0;
    int capacidade = 0;
    int valido = 1;

    memset(e, 0, sizeof *e);

    strcpy(e->algoritmo, PADRAO_ALGORITMO);
    e->quantum = PADRAO_QUANTUM;
    e->ncpus = PADRAO_CPUS;

    if (conteudo == NULL || *conteudo == '\0')
    {
        diag_erro(d, "arquivo vazio");
        return 0;
    }

    if (strncmp(conteudo, "\xEF\xBB\xBF", 3) == 0)
        conteudo += 3;

    copia = malloc(strlen(conteudo) + 1);

    if (copia == NULL)
    {
        diag_erro(d, "falta de memoria ao ler o arquivo");
        return 0;
    }

    strcpy(copia, conteudo);

    /* copia e quebrada em linhas no lugar ('\n' -> '\0') */
    for (linha = copia; linha != NULL; linha = prox)
    {
        char *campos[MAX_CAMPOS];
        char *txt;
        int n, i, ok = 1;
        TCB t;

        prox = strchr(linha, '\n');

        if (prox != NULL)
            *prox++ = '\0';

        num++;

        txt = trim(linha);

        if (*txt == '\0' || *txt == '#')
            continue;

        n = split_campos(txt, campos, MAX_CAMPOS);

        for (i = 0; i < MAX_CAMPOS; i++)
            campos[i] = (i < n) ? trim(campos[i]) : vazio;

        if (!tem_cabecalho)
        {
            /* primeira linha nao vazia/comentario = cabecalho, nao tarefa */
            tem_cabecalho = 1;

            if (strlen(campos[0]) >= sizeof e->algoritmo)
            {
                diag_erro(d, "linha %d: nome de algoritmo longo demais \"%s\"",
                          num, campos[0]);
                valido = 0;
            }
            else if (*campos[0] != '\0')
            {
                str_maiuscula(campos[0]);
                strcpy(e->algoritmo, campos[0]);
            }

            e->quantum = campo_int_min(d, num, "quantum", campos[1],
                                       PADRAO_QUANTUM, 0, &valido);

            e->ncpus = campo_int_min(d, num, "qtde_cpus", campos[2],
                                     PADRAO_CPUS, 1, &valido);

            continue;
        }

        memset(&t, 0, sizeof t);

        t.cpu = -1;

        t.periodo = campo_int_min(d, num, "periodo", campos[4],
                                  PADRAO_PERIODO, 0, &ok);

        /* tarefa aperiodica nao e simulada - descartada, nao e erro (4.4).
         * São dois avisos diferentes: se o usuário ESCREVEU 0, a mensagem
         * cita o 0; se deixou em branco, diz que o 0 veio do valor padrão,
         * para não apontar um número que não está no arquivo. */
        if (ok && t.periodo == 0)
        {
            if (campos[4] == NULL || *campos[4] == '\0')
                diag_aviso(d,
                           "linha %d: tarefa \"%s\": periodo nao informado; "
                           "padrao 0 (aperiodica), tarefa ignorada",
                           num, campos[0]);
            else
                diag_aviso(d,
                           "linha %d: tarefa \"%s\" e aperiodica (periodo 0) e foi ignorada",
                           num, campos[0]);
            continue;
        }

        t.id = campo_int_min(d, num, "id", campos[0],
                             -1, 0, &ok);

        if (!campo_cor(campos[1], &t))
        {
            diag_erro(d,
                      "linha %d: cor invalida \"%s\" (esperado RRGGBB em hexadecimal)",
                      num, campos[1]);
            ok = 0;
        }

        t.ingresso = campo_int_min(d, num, "ingresso", campos[2],
                                   0, 0, &ok);

        t.duracao = campo_int_min(d, num, "duracao", campos[3],
                                  0, 1, &ok);

        if (t.periodo > 0)
        {
            int ok_prazo = 1;

            t.prazo = campo_int_min(d, num, "prazo", campos[5],
                                    t.periodo, 0, &ok_prazo);

            if (!ok_prazo)
            {
                ok = 0;
            }
            else if (t.prazo == 0)
            {
                /* prazo omitido (0) -> default = periodo */
                diag_aviso(d,
                           "linha %d: tarefa \"%s\" com prazo 0; assumido prazo = periodo (%d)",
                           num, campos[0], t.periodo);

                t.prazo = t.periodo;
            }
        }

        /* campo 7 (eventos) leva o resto da linha; aqui so poda ';'/espacos no fim */
        {
            size_t tam = strlen(campos[6]);

            while (tam > 0 &&
                   (campos[6][tam - 1] == ';' ||
                    isspace((unsigned char)campos[6][tam - 1])))
            {
                campos[6][--tam] = '\0';
            }

            if (tam >= sizeof t.eventos)
            {
                diag_aviso(d,
                           "linha %d: lista de eventos longa demais; guardados so os primeiros %d caracteres",
                           num, (int)sizeof t.eventos - 1);
            }

            snprintf(t.eventos, sizeof t.eventos, "%s", campos[6]);
        }

        /* id duplicado invalida a tarefa (mas nao aborta o parse das demais) */
        for (i = 0; ok && i < e->ntarefas; i++)
        {
            if (e->tarefas[i].id == t.id)
            {
                diag_erro(d, "linha %d: id %d repetido", num, t.id);
                ok = 0;
            }
        }

        if (!ok)
        {
            valido = 0;
            continue;
        }

        /* vetor de tarefas cresce dobrando a capacidade (amortizado O(1)) */
        if (e->ntarefas == capacidade)
        {
            int nova = capacidade ? capacidade * 2 : 16;

            TCB *maior = realloc(e->tarefas,
                                 (size_t)nova * sizeof *maior);

            if (maior == NULL)
            {
                diag_erro(d, "falta de memoria na linha %d", num);
                valido = 0;
                break;
            }

            e->tarefas = maior;
            capacidade = nova;
        }

        e->tarefas[e->ntarefas++] = t;
    }

    free(copia);

    if (!tem_cabecalho)
    {
        diag_erro(d,
                  "arquivo sem a linha de parametros (algoritmo;quantum;qtde_cpus)");
        valido = 0;
    }
    else if (valido && e->ntarefas == 0)
    {
        diag_erro(d, "nenhuma tarefa periodica para simular");
        valido = 0;
    }

    /* so aloca e->cpus (qtde definida no cabecalho) se tudo ate aqui foi valido */
    if (valido)
    {
        e->cpus = calloc((size_t)e->ncpus, sizeof *e->cpus);

        if (e->cpus == NULL)
        {
            diag_erro(d, "falta de memoria para %d CPUs", e->ncpus);
            valido = 0;
        }
    }

    if (!valido)
    {
        estado_liberar(e);
        return 0;
    }

    for (int i = 0; i < e->ncpus; i++)
    {
        e->cpus[i].id = i;
        e->cpus[i].tarefa = -1;
    }

    return 1;
}