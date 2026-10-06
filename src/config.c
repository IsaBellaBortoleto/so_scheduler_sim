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
 * DECISÕES 
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

/* Compara dois textos sem diferenciar maiúsculas: "rm" == "RM" (req. 3.3.2).
 * Devolve 1 se iguais, 0 se diferentes (o contrário do strcmp). */
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

/* Converte o texto de um campo em número.
 * Campo vazio  => devolve `padrao`, sem erro (req. 3.2).
 * Texto ruim   => devolve 0 e põe 0 em *ok.
 *
 * Por que strtol e não atoi: o strtol informa ONDE parou de ler. É assim que
 * "3x" e "3.5" são recusados; o atoi devolveria 3 sem reclamar.
 *
 * Número negativo é aceito aqui. Quem decide se pode é quem chama, que sabe
 * dar a mensagem certa (ex. "periodo invalido"). */
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
 * ROTEIRO
 *   1. preenche os valores padrão;
 *   2. pula o BOM e faz uma cópia do texto (o split escreve nela);
 *   3. para cada linha: apara, pula se estiver em branco, quebra nos ';';
 *   4. a 1ª linha útil é o cabeçalho; as outras são tarefas;
 *   5. no fim, cria o vetor de CPUs, todas desligadas.
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
