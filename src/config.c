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

/* Parte comum de diag_erro e diag_aviso: grava a mensagem em lista[*n]. */
static void diag_add(char lista[][MAX_DIAG_TXT], int *n, const char *fmt, va_list args)
{
    /* A lista tem tamanho fixo (MAX_DIAG). Cheia, as mensagens seguintes
     * são ignoradas: o usuário já tem muito o que corrigir. */
    if (*n >= MAX_DIAG)
        return;
    /* vsnprintf e não vsprintf: escreve no máximo MAX_DIAG_TXT bytes, então
     * uma mensagem longa é cortada em vez de passar do fim da linha. */
    vsnprintf(lista[*n], MAX_DIAG_TXT, fmt, args);
    (*n)++;
}

/* Acrescenta uma mensagem de ERRO à lista, formatada como no printf:
 *     diag_erro(d, "linha %d: quantum invalido \"%s\"", n, txt);
 * Erro reprova o arquivo. */
void diag_erro(Diagnostico *d, const char *fmt, ...)
{
    /* O '...' recebe os valores do formato, como no printf. va_start começa
     * a lê-los logo depois de 'fmt'; diag_add os repassa ao vsnprintf. */
    va_list args;
    va_start(args, fmt);
    diag_add(d->erros, &d->n_erros, fmt, args);
    /* Todo va_start precisa de um va_end, que encerra a leitura. */
    va_end(args);
}

/* Igual a diag_erro, mas na lista de AVISOS. Aviso NÃO reprova o arquivo:
 * é algo que o usuário precisa saber e que a simulação contorna sozinha
 * (ex.: tarefa aperiódica ignorada, req. 4.4). */
void diag_aviso(Diagnostico *d, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    diag_add(d->avisos, &d->n_avisos, fmt, args);
    va_end(args);
}

/* ------------------------------ utilitários ------------------------------ */

/* Compara dois textos sem diferenciar maiúsculas: "rm" == "RM" (req. 3.3.2).
 * Devolve 1 se iguais, 0 se diferentes (o contrário do strcmp). */
int str_igual_ci(const char *a, const char *b)
{
    /* NULL não é igual a nada (nem a outro NULL), e não pode ser lido. */
    if (a == NULL || b == NULL)
    {
        return 0;
    }
    /* Compara letra a letra até uma das duas acabar. */
    while (*a && *b)
    {
        /* O cast para unsigned char é obrigatório: um byte de letra
         * acentuada vira número negativo num char, e tolower com número
         * negativo é comportamento indefinido em C. */
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return 0;
        }

        a++;
        b++;
    }
    /* Iguais só se as DUAS acabaram juntas: "RM" e "RMS" param no 'S'. */
    return *a == *b;
}

/* Tira espaços, tabs e '\r' do começo e do fim (req. 3.3.6). Não copia
 * nada: corta o fim com '\0' e devolve um ponteiro para o começo de verdade.
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

    /* Início: avança o ponteiro até o primeiro caractere que não é espaço.
     * Nada é apagado; quem chama passa a usar este ponteiro. */
    char *inicio = s;

    while (*inicio && isspace((unsigned char)*inicio))
    {
        inicio++;
    }

    return inicio;
}

/* Converte o texto para maiúsculas, no próprio lugar (req. 3.3.2: "rm",
 * "Rm" e "RM" são o mesmo algoritmo). NULL é ignorado.
 * Só letras ASCII mudam; bytes de letras acentuadas em UTF-8 ficam como
 * estão. O cast para unsigned char é pelo mesmo motivo do str_igual_ci. */
static void str_maiuscula(char *s)
{
    if (s == NULL)
        return;
    /* O '\0' do fim do texto vale 0 (falso): o laço para nele. */
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
     * O n < max impede escrever além do fim de campos[]. */
    while (n < max)
    {
        /* O ÚLTIMO campo permitido não é cortado: fica com o resto da linha
         * inteiro, com os ';' que tiver (é a lista de eventos). */
        char *sep = (n == max - 1) ? NULL : strchr(linha, ';');

        if (sep == NULL)
        {
            /* Não há mais ';': o resto é o último campo. Se ele é vazio,
             * a linha terminava em ';' (ex.: "1;5;") e não há campo ali.
             * Um vazio no meio ("1;;5") já foi guardado antes. */
            if (*linha != '\0')
                campos[n++] = linha;

            break;
        }
        /* Troca o ';' por '\0': o pedaço até aqui vira um texto separado,
         * sem precisar copiar (campos[] só guarda onde cada um começa). */
        *sep = '\0';
        campos[n++] = linha;
        /* O próximo campo começa logo depois do ';'. */
        linha = sep + 1;
    }
    return n;
}

/* Converte 'txt' em inteiro. Devolve 1 se o texto INTEIRO é um número
 * ("12"), 0 se não é ("abc", "", "12x") ou não cabe num int
 * ("99999999999"). Usada aqui (campo_int) e na edição de tarefas (kernel.c).
 * strtol e não atoi: o atoi transforma "abc" em 0 sem avisar. */
int ler_inteiro(const char *txt, int *saida)
{
    /* O strtol diz em 'fim' onde parou de ler. */
    char *fim;
    long valor;

    /* O errno só é escrito quando há erro: zerado antes, um ERANGE depois
     * é com certeza deste strtol. */
    errno = 0;
    valor = strtol(txt, &fim, 10);

    /* fim == txt: nenhum dígito foi lido.
     * *fim != '\0': sobrou texto depois do número.
     * ERANGE/limites: número grande demais. */
    if (fim == txt || *fim != '\0' ||
        errno == ERANGE || valor < INT_MIN || valor > INT_MAX)
        return 0;

    *saida = (int)valor;
    return 1;
}

/* Converte o texto de um campo em número.
 * Campo vazio => devolve 'padrao', sem erro (req. 3.2).
 * Texto ruim => devolve 0 e põe 0 em *ok. */
static int campo_int(const char *txt, int padrao, int *ok)
{
    int valor = 0;

    *ok = 1;
    if (*txt == '\0')
        return padrao;

    *ok = ler_inteiro(txt, &valor);
    return valor;
}

/* Converte "F0E0D0" em três componentes RGB.
 * Requisito 3.3: "cor é cor que identifica a execução da tarefa no formato
 * RGB em Hexadecimal, p.ex. 'F0E0D0' indica Red=F0, Green=E0, Blue=D0".
 * Campo vazio => cor padrão (req. 3.2). */
static int campo_cor(const char *txt, TCB *t)
{
    unsigned int r, g, b;

    if (*txt == '\0')
    {
        t->r = PADRAO_COR_R;
        t->g = PADRAO_COR_G;
        t->b = PADRAO_COR_B;
        return 1;
    }

    /* '#' na frente é aceito, mas não exigido: quem está acostumado com
     * cores de páginas web costuma escrever "#F0E0D0". */
    if (*txt == '#')
        txt++;

    /* A cor deve ter exatamente 6 dígitos hexadecimais (0-9, A-F). */
    if (strlen(txt) != 6)
        return 0;

    for (int i = 0; i < 6; i++)
    {
        if (!isxdigit((unsigned char)txt[i]))
            return 0;
    }

    /* Exemplo: F0E0D0 -> F0, E0, D0. */
    sscanf(txt, "%2x%2x%2x", &r, &g, &b);
    /* O sscanf precisa de unsigned int, mas o TCB guarda cada cor em 1 byte.
     * A conversão é segura: com 2 dígitos hexadecimais o maior valor é FF
     * (255), que cabe em 1 byte. */
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
 * assim vários campos da mesma linha acumulam no mesmo *ok).
 * A mensagem diz a linha, o campo, o que foi lido e o que era esperado
 * (req. 3.3.6). */
static int campo_int_min(Diagnostico *d, int linha, const char *nome,
                         const char *txt, int padrao, int minimo, int *ok)
{
    int leu;
    int valor = campo_int(txt, padrao, &leu);

    /* Dois motivos de erro: não é número ("abc"), ou é número mas abaixo
     * do mínimo (ex.: duração 0, período -5). */
    if (!leu || valor < minimo)
    {
        diag_erro(d, "linha %d: %s invalido \"%s\" (esperado um inteiro >= %d)",
                  linha, nome, txt, minimo);
        *ok = 0;
    }

    return valor;
}

/* Lê o texto de configuração linha a linha e preenche *e.
 *   1ª linha válida = cabeçalho (algoritmo;quantum;qtde_cpus)
 *   demais linhas   = uma tarefa (TCB) cada
 * 'tem_cabecalho' diz em qual das duas partes a leitura está.
 * Devolve 1 se carregou sem erros; senão 0, com tudo já liberado e os
 * erros em d. */
int config_carregar_texto(const char *conteudo, Estado *e, Diagnostico *d)
{
    char vazio[1] = "";
    char *copia, *linha, *prox;
    int num = 0;
    int tem_cabecalho = 0;
    int capacidade = 0;
    int valido = 1;
    /* Começa tudo zerado: ponteiros NULL e contadores 0. */
    memset(e, 0, sizeof *e);

    /* Os padrões valem até o cabeçalho dizer outra coisa (req. 3.2). */
    strcpy(e->algoritmo, PADRAO_ALGORITMO);
    e->quantum = PADRAO_QUANTUM;
    e->ncpus = PADRAO_CPUS;

    if (conteudo == NULL || *conteudo == '\0')
    {
        diag_erro(d, "arquivo vazio");
        return 0;
    }

    /* BOM: o Bloco de Notas e outros editores podem gravar 3 bytes
     * invisíveis (EF BB BF) no começo de um arquivo UTF-8. Sem pular, o
     * algoritmo seria lido como "\xEF\xBB\xBFRM" e o RM do arquivo seria
     * acusado de inexistente. */
    if (strncmp(conteudo, "\xEF\xBB\xBF", 3) == 0)
        conteudo += 3;

    /* A leitura corta o texto no lugar (troca '\n' e ';' por '\0'), e o
     * 'conteudo' é const: por isso uma cópia. */
    copia = malloc(strlen(conteudo) + 1);

    if (copia == NULL)
    {
        diag_erro(d, "falta de memoria ao ler o arquivo");
        return 0;
    }

    strcpy(copia, conteudo);

    /* Uma linha por volta: strchr acha o '\n', que vira '\0', e 'prox'
     * aponta para o começo da linha seguinte. */
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
        /* Linha em branco ou comentário ('#'): pula (req. 3.3.6). */
        if (*txt == '\0' || *txt == '#')
            continue;

        n = split_campos(txt, campos, MAX_CAMPOS);
        /* Apara cada campo; os que faltam viram "" (valor padrão). */
        for (i = 0; i < MAX_CAMPOS; i++)
            campos[i] = (i < n) ? trim(campos[i]) : vazio;

        /* ----------------------------- cabeçalho ----------------------------- */
        if (!tem_cabecalho)
        {
            /* primeira linha nao vazia/comentario = cabecalho, nao tarefa */
            tem_cabecalho = 1;

            /* O nome vai para um vetor de 16 posições: maior que isso seria
             * escrito além do fim dele. */
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

        /* ------------------------------ tarefas ------------------------------ */
        memset(&t, 0, sizeof t);

        /* Começa sem CPU. */
        t.cpu = -1;

        t.periodo = campo_int_min(d, num, "periodo", campos[4],
                                  PADRAO_PERIODO, 0, &ok);

        /* tarefa aperiodica nao e simulada - descartada, nao e erro (4.4).
         * São dois avisos diferentes: se o usuário ESCREVEU 0, a mensagem
         * cita o 0; se deixou em branco, diz que o 0 veio do valor padrão,
         * para não apontar um número que não está no arquivo. */
        if (ok && t.periodo == 0)
        {
            if (*campos[4] == '\0')
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
                /* Prazo 0 não faz sentido (venceria na própria chegada).
                 * Vale o período, como no RM (Di = Pi), e o usuário é
                 * avisado. Recusar o arquivo impediria a simulação. */
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
            /* O TCB guarda até 255 caracteres de eventos; o que passar
             * disso é cortado, com aviso. */
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
        /* Algum campo desta linha falhou: a tarefa fica de fora, mas a
         * leitura continua, para mostrar os erros das outras linhas. */
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

    /* ---------------------------- verificação final -------------------------- */

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
        /* calloc já devolve tudo zerado (ticks_desligada = 0). */
        e->cpus = calloc((size_t)e->ncpus, sizeof *e->cpus);

        if (e->cpus == NULL)
        {
            diag_erro(d, "falta de memoria para %d CPUs", e->ncpus);
            valido = 0;
        }
    }
    /* Com erro, libera o que já foi alocado: quem chamou não recebe nada. */
    if (!valido)
    {
        estado_liberar(e);
        return 0;
    }
    /* Cada CPU começa com o seu número e sem tarefa (-1 = desligada). */
    for (int i = 0; i < e->ncpus; i++)
    {
        e->cpus[i].id = i;
        e->cpus[i].tarefa = -1;
    }

    return 1;
}
