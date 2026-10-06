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
 *   3.3.4 qualquer lugar, inclusive pendrive . o C recebe o CONTEÚDO, escolhido
 *                                              pelo seletor de arquivo da página
 *   3.3.6 espaços e linhas em branco não são erro ... trim + salto de linha vazia
 *   3.3.6 erros claros com o motivo .......... diag_erro("linha %d: ...")
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
    va_list args;

    if (d->n_erros >= MAX_DIAG)
        return;

    /* va_list é o jeito de repassar os "..." para outra função. O vsnprintf
     * é o snprintf que recebe va_list; o tamanho impede estourar a linha da
     * matriz: mensagem longa demais é cortada, sempre com '\0' no fim. */
    va_start(args, fmt);
    vsnprintf(d->erros[d->n_erros], sizeof d->erros[d->n_erros], fmt, args);
    va_end(args);

    d->n_erros++;
}

/* Igual a diag_erro, mas na lista de AVISOS. Aviso não reprova o arquivo: é
 * para o que o usuário precisa saber e a simulação consegue contornar (ex.
 * tarefa aperiódica ignorada, requisito 4.4). */
void diag_aviso(Diagnostico *d, const char *fmt, ...)
{
    va_list args;

    if (d->n_avisos >= MAX_DIAG)
        return;

    va_start(args, fmt);
    vsnprintf(d->avisos[d->n_avisos], sizeof d->avisos[d->n_avisos], fmt, args);
    va_end(args);

    d->n_avisos++;
}

/* ------------------------------ utilitários ------------------------------ */

int str_igual_ci(const char *a, const char *b)
{
    if(a==NULL || b==NULL)
    {
        return 0;
    }
    while(*a && *b)
    {
        /* O cast para unsigned char é obrigatório: tolower com char negativo
         * (byte de letra acentuada, onde char é signed) é comportamento
         * indefinido. */
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return 0;
        }
        a++;
        b++;
    }

    /* Saiu do laço porque pelo menos uma acabou: só são iguais se as DUAS
     * acabaram ('\0' == '\0'). Sem isso, "RM" seria igual a "RMS". */
    return *a == *b;
}

/* Remove espaços, tabs e quebras de linha ('\r', '\n') do início e do fim,
 * IN PLACE. Requisito 3.3.6: "espaços e linhas em branco não representam
 * erros". 's' não pode ser NULL.
 *
 * USE O RETORNO, não 's': o fim é cortado escrevendo '\0' na própria string,
 * mas o início é só "pulado" -- a função devolve um ponteiro para o primeiro
 * caractere útil, dentro do mesmo buffer (nada é copiado nem alocado, então
 * não há free). String só de espaços devolve "".
 *
 * BUG CLÁSSICO: aparar a linha inteira não apara os campos. Em " 1 ; 5 " o
 * trim da linha tira só as pontas; depois do split os campos ainda são "1 "
 * e " 5". Por isso o trim é chamado de novo em CADA campo. */
static char *trim(char *s)
{
    /* Fim primeiro: anda de trás para frente trocando cada espaço por '\0'.
     * O tam > 0 vem antes para nunca ler s[-1] numa string vazia. */
    int tam = strlen(s);
    while (tam > 0 && isspace((unsigned char)s[tam - 1])) {
        s[tam - 1] = '\0';
        tam--;
    }

    /* Início: avança o ponteiro até o primeiro caractere que não é espaço. */
    char *inicio = s;
    while (*inicio && isspace((unsigned char)*inicio)) {
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
    int n = 0;

    /* 'linha' anda pelo texto: aponta sempre para o começo do próximo campo.
     * O n < max impede estourar campos[]. */
    while (n < max)
    {
        /* O ÚLTIMO campo permitido não é cortado: fica com o resto da linha
         * inteiro, com os ';' que tiver. É o caso da lista de eventos, que
         * pode trazer vários eventos separados por ';' (req. 3.3.5). */
        char *sep = (n == max - 1) ? NULL : strchr(linha, ';');
        if (sep == NULL)
        {
            /* Não há mais ';': o resto é o último campo. Se ele é vazio, a
             * linha terminava em ';' (ou era vazia) e o campo é descartado
             * (req. 3.3.3). Só aqui: um vazio no MEIO ("1;;5") é guardado. */
            if (*linha != '\0')
                campos[n++] = linha;
            break;
        }
        *sep = '\0'; /* corta o campo no lugar do ';' */
        campos[n++] = linha;
        linha = sep + 1;
    }

    return n;
}

/* Lê um inteiro validando que o texto inteiro é numérico.
 * Campo vazio => devolve `padrao` (requisito 3.2).
 * Texto inválido => devolve 0 e sinaliza erro em *ok. */
static int campo_int(const char *txt, int padrao, int *ok)
{
    char *fim;
    long valor;

    *ok = 1;
    if (txt == NULL || *txt == '\0')
        return padrao;

    errno = 0;
    valor = strtol(txt, &fim, 10);

    /* fim == txt:   nenhum dígito foi lido ("abc").
     * *fim != '\0': sobrou lixo depois do número ("12abc").
     * ERANGE/limites: número grande demais ("99999999999"). Sem essa checagem
     * o strtol devolve LONG_MAX e o arquivo errado passaria como válido. */
    if (fim == txt || *fim != '\0' || errno == ERANGE || valor < INT_MIN || valor > INT_MAX) {
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
    unsigned int r, g, b; /* o %x do sscanf exige unsigned int, não unsigned char */

    /* Campo vazio: cor padrão (requisito 3.2). */
    if (txt == NULL || *txt == '\0')
    {
        t->r = PADRAO_COR_R;
        t->g = PADRAO_COR_G;
        t->b = PADRAO_COR_B;
        return 1;
    }

    if (*txt == '#') /* '#' na frente é opcional */
        txt++;

    /* Exatamente 6 dígitos hexadecimais. A validação vem ANTES do sscanf
     * porque ele sozinho aceitaria "F0E0D0lixo", espaços e sinal. */
    if (strlen(txt) != 6)
        return 0;
    for (int i = 0; i < 6; i++)
    {
        if (!isxdigit((unsigned char)txt[i]))
            return 0;
    }

    /* %2x lê dois dígitos hexadecimais por vez: "F0E0D0" -> F0, E0, D0. */
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
    if (!leu || valor < minimo) {
        diag_erro(d, "linha %d: %s invalido \"%s\" (esperado um inteiro >= %d)",
                  linha, nome, txt, minimo);
        *ok = 0;
    }
    return valor;
}

/* Interpreta o texto do arquivo de configuração e preenche *e.
 * Retorna 1 se carregou; 0 se houve erro (mensagens em d), e nesse caso *e
 * fica vazio, sem nada para liberar.
 *
 * Não para no primeiro erro: cada linha ruim é reportada e a leitura segue
 * (por isso os `continue`), para o usuário corrigir tudo de uma vez. */
int config_carregar_texto(const char *conteudo, Estado *e, Diagnostico *d)
{
    char vazio[1] = "";
    char *copia, *linha, *prox;
    int num = 0;            /* número da linha, para as mensagens */
    int tem_cabecalho = 0;
    int capacidade = 0;     /* tamanho alocado de e->tarefas */
    int valido = 1;

    /* Valores padrão (requisito 3.2); o arquivo sobrescreve campo a campo. */
    memset(e, 0, sizeof *e);
    strcpy(e->algoritmo, PADRAO_ALGORITMO);
    e->quantum = PADRAO_QUANTUM;
    e->ncpus = PADRAO_CPUS;

    if (conteudo == NULL || *conteudo == '\0') {
        diag_erro(d, "arquivo vazio");
        return 0;
    }

    /* BOM: alguns editores salvam "UTF-8 com BOM", 3 bytes invisíveis no
     * começo. Sem pular, o algoritmo viraria "\xEF\xBB\xBFRM" e o erro
     * mostraria um RM aparentemente correto como inexistente. */
    if (strncmp(conteudo, "\xEF\xBB\xBF", 3) == 0)
        conteudo += 3;

    /* 'conteudo' é const e o split escreve '\0' no texto: trabalha numa
     * cópia. Sem tamanho máximo de linha, ao contrário do fgets. */
    copia = malloc(strlen(conteudo) + 1);
    if (copia == NULL) {
        diag_erro(d, "falta de memoria ao ler o arquivo");
        return 0;
    }
    strcpy(copia, conteudo);

    for (linha = copia; linha != NULL; linha = prox) {
        char *campos[MAX_CAMPOS];
        char *txt;
        int n, i, ok = 1;
        TCB t;

        /* Corta a linha no '\n' e já guarda onde começa a próxima. */
        prox = strchr(linha, '\n');
        if (prox != NULL)
            *prox++ = '\0';
        num++;

        /* Linha em branco não é erro (3.3.6); o trim também tira o '\r' de
         * arquivos do Windows (CRLF). '#' inicia comentário. */
        txt = trim(linha);
        if (*txt == '\0' || *txt == '#')
            continue;

        /* Campo que não veio na linha vale "" (= usar o padrão). O trim é
         * refeito em CADA campo: ver o comentário em trim(). */
        n = split_campos(txt, campos, MAX_CAMPOS);
        for (i = 0; i < MAX_CAMPOS; i++)
            campos[i] = (i < n) ? trim(campos[i]) : vazio;

        /* ---- primeira linha útil: algoritmo;quantum;qtde_cpus ---- */
        if (!tem_cabecalho) {
            tem_cabecalho = 1;
            if (strlen(campos[0]) >= sizeof e->algoritmo) {
                diag_erro(d, "linha %d: nome de algoritmo longo demais \"%s\"", num, campos[0]);
                valido = 0;
            } else if (*campos[0] != '\0') {
                str_maiuscula(campos[0]);
                strcpy(e->algoritmo, campos[0]);
            }
            /* Se o algoritmo existe quem confere é o sim_iniciar, que
             * conhece a tabela de escalonadores.
             * Quantum 0 é aceito e significa "sem limite de quantum": a
             * tarefa só sai da CPU quando termina ou quando chega outra mais
             * prioritária (ver executar() no kernel). O enunciado define o
             * quantum como o tempo máximo de execução; 0 ao pé da letra seria
             * "nunca executa", o que não faz sentido simular. */
            e->quantum = campo_int_min(d, num, "quantum", campos[1], PADRAO_QUANTUM, 0, &valido);
            e->ncpus = campo_int_min(d, num, "qtde_cpus", campos[2], PADRAO_CPUS, 1, &valido);
            continue;
        }

        /* ---- demais linhas: uma tarefa ---- */
        memset(&t, 0, sizeof t); /* estado dinâmico zerado: EST_INATIVA etc. */
        t.cpu = -1;

        /* O período vem primeiro: tarefa aperiódica (período 0) é ignorada
         * com AVISO (requisito 4.4), sem validar o resto da linha. */
        t.periodo = campo_int_min(d, num, "periodo", campos[4], 0, 0, &ok);
        if (ok && t.periodo == 0) {
            diag_aviso(d, "linha %d: tarefa \"%s\" e aperiodica (periodo 0) e foi ignorada",
                       num, campos[0]);
            continue;
        }

        t.id = campo_int_min(d, num, "id", campos[0], -1, 0, &ok);
        if (!campo_cor(campos[1], &t)) {
            diag_erro(d, "linha %d: cor invalida \"%s\" (esperado RRGGBB em hexadecimal)",
                      num, campos[1]);
            ok = 0;
        }
        t.ingresso = campo_int_min(d, num, "ingresso", campos[2], 0, 0, &ok);
        t.duracao = campo_int_min(d, num, "duracao", campos[3], 0, 1, &ok);
        if (t.periodo > 0) {
            /* Prazo vazio ou 0 = o próprio período (deadline igual ao
             * período, a premissa do RM). O 0 escrito gera AVISO, e não erro:
             * o enunciado não proíbe, e recusar o arquivo inteiro por isso
             * impediria a demonstração (req. 3.3.7). */
            int ok_prazo = 1;
            t.prazo = campo_int_min(d, num, "prazo", campos[5], t.periodo, 0, &ok_prazo);
            if (!ok_prazo) {
                ok = 0;
            } else if (t.prazo == 0) {
                diag_aviso(d, "linha %d: tarefa \"%s\" com prazo 0; assumido prazo = periodo (%d)",
                           num, campos[0], t.periodo);
                t.prazo = t.periodo;
            }
        }

        /* Eventos: só são interpretados no Projeto B; aqui guardamos o texto.
         * Como é o último campo, ele vem com o resto da linha inteiro. O ';'
         * final é opcional (req. 3.3.3), então é removido. */
        {
            size_t tam = strlen(campos[6]);
            while (tam > 0 && (campos[6][tam - 1] == ';' || isspace((unsigned char)campos[6][tam - 1])))
                campos[6][--tam] = '\0';
            if (tam >= sizeof t.eventos)
                diag_aviso(d, "linha %d: lista de eventos longa demais; guardados so os primeiros %d caracteres",
                           num, (int)sizeof t.eventos - 1);
            snprintf(t.eventos, sizeof t.eventos, "%s", campos[6]);
        }

        for (i = 0; ok && i < e->ntarefas; i++) {
            if (e->tarefas[i].id == t.id) {
                diag_erro(d, "linha %d: id %d repetido", num, t.id);
                ok = 0;
            }
        }

        if (!ok) {
            valido = 0;
            continue;
        }

        /* Sem limite de tarefas (3.3.1): o vetor dobra quando enche. */
        if (e->ntarefas == capacidade) {
            int nova = capacidade ? capacidade * 2 : 16;
            TCB *maior = realloc(e->tarefas, (size_t)nova * sizeof *maior);
            if (maior == NULL) {
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

    if (!tem_cabecalho) {
        diag_erro(d, "arquivo sem a linha de parametros (algoritmo;quantum;qtde_cpus)");
        valido = 0;
    } else if (valido && e->ntarefas == 0) {
        diag_erro(d, "nenhuma tarefa periodica para simular");
        valido = 0;
    }

    /* Processadores: todos começam desligados (tarefa = -1). */
    if (valido) {
        e->cpus = calloc((size_t)e->ncpus, sizeof *e->cpus);
        if (e->cpus == NULL) {
            diag_erro(d, "falta de memoria para %d CPUs", e->ncpus);
            valido = 0;
        }
    }
    if (!valido) {
        estado_liberar(e);
        return 0;
    }
    for (int i = 0; i < e->ncpus; i++) {
        e->cpus[i].id = i;
        e->cpus[i].tarefa = -1;
    }
    return 1;
}
