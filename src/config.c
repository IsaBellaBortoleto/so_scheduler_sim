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

/* Acrescenta uma mensagem de erro à lista, formatada como no printf:
 *     diag_erro(d, "linha %d: quantum invalido \"%s\"", n, txt);
 * A lista tem tamanho fixo: depois de MAX_DIAG erros os seguintes são
 * ignorados (o arquivo já está reprovado de qualquer forma, n_erros > 0). */
/* Parte comum de diag_erro e diag_aviso: grava a mensagem em lista[*n]. */
static void diag_add(char lista[][MAX_DIAG_TXT], int *n, const char *fmt, va_list args)
{
    /* Proteção contra Out-of-Bounds (Acesso indevido a índices do array).
     * Se já atingimos o limite de mensagens que a interface web consegue
     * exibir, ignoramos o resto silenciosamente. */
    if (*n >= MAX_DIAG)
        return;
    /* Escrita segura: o vsnprintf garante que, no máximo, MAX_DIAG_TXT
     * bytes serão escritos na string de destino, prevenindo Buffer
     * Overflows se a mensagem gerada for excessivamente longa. */
    vsnprintf(lista[*n], MAX_DIAG_TXT, fmt, args);
    (*n)++;
}

void diag_erro(Diagnostico *d, const char *fmt, ...)
{
    /* TEORIA DE FUNÇÕES VARIÁDICAS (va_list):
     * Em C, '...' na assinatura da função significa que ela aceita um número 
     * indefinido de argumentos (como no printf). 
     * 'va_list' é o tipo usado para armazenar esses argumentos extras.
     * O 'va_start' diz ao compilador: "Comece a ler os argumentos variáveis 
     * logo após o parâmetro 'fmt'". */

    va_list args;
    va_start(args, fmt);
    /* Monta a mensagem final fundindo a string base (fmt) com os argumentos (args) */
    diag_add(d->erros, &d->n_erros, fmt, args);
    /* O 'va_end' limpa e reorganiza o ponteiro da pilha (stack) da CPU.
     * Se esquecido, pode causar vazamento de recursos ou travamento. */
    va_end(args);
}
/* ============================================================================
 * Requisito 4.4
 * Igual a diag_erro, mas na lista de AVISOS. 
 *
 * TEORIA DE TOLERÂNCIA A FALHAS E SISTEMAS DE TEMPO REAL:
 * Um aviso (Warning) não reprova o arquivo. Ele serve para reportar anomalias
 * que o sistema consegue contornar de forma segura (Graceful Degradation).*/

void diag_aviso(Diagnostico *d, const char *fmt, ...)
{
    /* Preparação da pilha (stack) para ler os argumentos variáveis (...) */
    va_list args;
    va_start(args, fmt);
    diag_add(d->avisos, &d->n_avisos, fmt, args);
    //Limpeza obrigatória do ponteiros de argumentos
    va_end(args);
}
/* ------------------------------ utilitários ------------------------------ */
//Cumpre o requisito 3.3.2 
/* Compara dois textos sem diferenciar maiúsculas: "rm" == "RM" (req. 3.3.2).
 * Devolve 1 se iguais, 0 se diferentes (o contrário do strcmp). */
int str_igual_ci(const char *a, const char *b)
{
    //Proteção para ponteiros nulos. Eviando Segmentation Faults
    if (a == NULL || b == NULL)
    {
        return 0;
    }
    /* O ciclo avança até que uma das strings chegue ao fim ('\0') */
    while (*a && *b)
    {
        /* O cast para unsigned char é obrigatório: tolower com char negativo
         * (byte de letra acentuada, onde char é signed) é comportamento
         * indefinido. Passar um número negativo (exceto o macro EOF) para 'tolower' resulta
         * num Undefined Behavior crítico, podendo crashar a aplicação.
         * O cast para (unsigned char) força o valor a ser interpretado de 
         * 0 a 255, garantindo total segurança na conversão. */
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return 0;
        }

        a++;
        b++;
    }
    /*Só serão verdadeiramente iguais se AMBAS tiverem chegado ao fim ('\0')
     * exatamente ao mesmo tempo, por isso a verificação novamente. */
    return *a == *b;
}

/* Requisito 3.3.6 
 * Tira espaços, tabs e '\r' do começo e do fim. Não copia nada:
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

    /* Início: avança o ponteiro até o primeiro caractere que não é espaço. 
    * criamos um novo ponteiro que passa pela string até
    * encontrar o primeiro caractere que não seja um espaço. */
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
    /* Validação defensiva padrão: evita falhas de segmentação caso 
     * a função receba um ponteiro nulo acidentalmente. */    
    if (s == NULL)
        return;
    /*A condição 'while (*s)' lê o valor do byte na morada de memória atual. 
     * Em C, o terminador de string '\0' tem o valor numérico 0 (falso). 
     * Logo, este ciclo avança magicamente até encontrar o fim da string.*/
    while (*s)
    {
        
        *s = toupper((unsigned char)*s);
        /* Avança o ponteiro para a próxima morada de memória (próximo caractere) */
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

    /* O ponteiro 'linha' desliza pela string, apontando sempre para o 
     * começo do próximo campo a ser processado.
     * A proteção 'n < max' previne Buffer Overflow (estouro do array de 
     * ponteiros 'campos[]'). */
    while (n < max)
    {
        /* O ÚLTIMO campo permitido não é cortado: fica com o resto da linha
         * inteiro, com os ';' que tiver. */
        char *sep = (n == max - 1) ? NULL : strchr(linha, ';');

        if (sep == NULL)
        {
            /* Condição de paragem: Não há mais ';' na string (ou forçámos 
             * a paragem na regra do último campo acima).
             * 
             * Tratamento do Req. 3.3.3 (Trailing Semicolon):
             * Se a string terminava em ';' (ex: "1;5;"), o ponteiro 'linha' 
             * foi parar exatamente no '\0'. Nesse caso, o if (*linha != '\0') 
             * ignora este "resto" vazio, destruindo o campo fantasma final.
             * Porém, um vazio no MEIO da string (guardado em iterações 
             * anteriores) já foi preservado em segurança. */
            if (*linha != '\0')
                campos[n++] = linha;

            break;
        }
        /*
         * Em vez de fazer malloc() para copiar o texto da coluna para um
         * novo lugar na memória, substituímos cirurgicamente o caractere ';'
         * por um terminador de string '\0'. 
         * Assim, o array 'campos' apenas guarda ponteiros que apontam para 
         * moradas diferentes dentro da mesma string original. */
        *sep = '\0';
        campos[n++] = linha;
        /* O próximo campo começa exatamente 1 byte após o ponto-e-vírgula 
         * que acabámos de destruir. */
        linha = sep + 1;
    }
    /* Retorna quantas colunas 
     * esta linha efetivamente continha. */
    return n;
}

/* Converte 'txt' em inteiro. Devolve 1 se o texto INTEIRO é um número
 * ("12"), 0 se não é ("abc", "", "12x") ou não cabe num int
 * ("99999999999"). Usada aqui (campo_int) e na edição de tarefas (kernel.c). */
int ler_inteiro(const char *txt, int *saida)
{
    /* O strtol vai alterar este ponteiro para apontar para o exato caractere
     * onde ele parou de ler o número. */
    char *fim;
    long valor;

    /* Reseta a flag de erro global da thread (errno) do Sistema Operativo
     * ANTES de chamar a função, garantindo que não estamos a ler um erro
     * antigo deixado por outra função. */
    errno = 0;
    /* Converte a string para long usando a base 10 (decimal). */
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
 * Campo vazio => devolve 'padrao', sem erro.
 * Texto ruim => devolve 0 e põe 0 em *ok.
 * Converte o texto de um campo em número inteiro.*/
static int campo_int(const char *txt, int padrao, int *ok)
{
    int valor = 0;

    /* REQUISITO 3.2
     * Se o campo estiver totalmente em branco, não é um erro. O sistema
     * aplica silenciosamente a configuração padrão passada pelo chamador. */
    *ok = 1;
    if (*txt == '\0')
        return padrao;

    *ok = ler_inteiro(txt, &valor);
    return valor;
}

/* Converte "F0E0D0" em três componentes RGB.
 * Requisito 3.3 e 3.2: "cor é cor que identifica a execução da tarefa no formato
 * RGB em Hexadecimal, p.ex. 'F0E0D0' indica Red=F0, Green=E0, Blue=D0". */
static int campo_cor(const char *txt, TCB *t)
{
    unsigned int r, g, b;

    /* Tratamento de ausência de dados */
    if (*txt == '\0')
    {
        t->r = PADRAO_COR_R;
        t->g = PADRAO_COR_G;
        t->b = PADRAO_COR_B;
        return 1;
    }

    /* 
     * Na web, as cores hexadecimais são precedidas pela cerquilha '#'.
     * O enunciado não pede o '#', mas utilizadores habituados a ficheiros 
     * de configuração web colocam-no instintivamente. 
     * O 'txt++' é pura aritmética de ponteiros: avançamos o ponteiro 1 byte 
     * para a frente, ignorando o '#' e lendo apenas os números reais a seguir. */
    if (*txt == '#')
        txt++;

    /* A cor deve ter exatamente 6 dígitos hexadecimais. */
    if (strlen(txt) != 6)
        return 0;

    /*  Verifica se todos os caracteres pertencem ao conjunto [0-9, A-F, a-f].
     *  O cast (unsigned char) protege contra falhas no isxdigit. */    
    for (int i = 0; i < 6; i++)
    {
        if (!isxdigit((unsigned char)txt[i]))
            return 0;
    }

    /* Exemplo: F0E0D0 -> F0, E0, D0. */
    sscanf(txt, "%2x%2x%2x", &r, &g, &b);
    /* 
     * O sscanf com %x exige ponteiros para inteiros (no mínimo 32-bits em x86).
     * No entanto, a nossa estrutura TCB guarda as cores em 'unsigned char' (8-bits) 
     * para poupar memória RAM.
     * Como lemos '%2x', o valor máximo absoluto garantido é 'FF' (255 em decimal).
     * O número 255 cabe perfeitamente em 8-bits, tornando este 'cast' explícito 
     * matematicamente seguro e livre de perdas de dados (truncation). */
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
 * 
 * LIGAÇÃO AOS REQUISITOS (Req 3.3.6):
 * Centraliza a geração de mensagens claras, indicando exatamente a linha do 
 * ficheiro, o nome da coluna que falhou, o que foi lido e qual era o limite esperado*/
static int campo_int_min(Diagnostico *d, int linha, const char *nome,
                         const char *txt, int padrao, int minimo, int *ok)
{
    int leu;
    /* Reutiliza o conversor seguro de baixo nível (strtol) que criámos antes.
     * A variável 'leu' vai informar-nos se a conversão do texto para bits ocorreu. */
    int valor = campo_int(txt, padrao, &leu);
    /* 
     * A função dispara o erro em dois cenários distintos fundidos num só 'if':
     * 1. !leu: Ocorreu uma falha sintática. O strtol encontrou lixo ("abc").
     * 2. valor < minimo: Ocorreu uma falha semântica. O número é um inteiro válido,
     *    mas viola as leis do simulador  */

    if (!leu || valor < minimo)
    {
        /* Injeta o diagnóstico formatado na nossa matriz de erros */
        diag_erro(d, "linha %d: %s invalido \"%s\" (esperado um inteiro >= %d)",
                  linha, nome, txt, minimo);
        *ok = 0;
    }

    return valor;
}

/* ============================================================================
 * Parseia o texto de configuracao linha a linha.
 * 1a linha valida = cabecalho (algoritmo;quantum;qtde_cpus)
 * demais linhas = tarefas (TCBs).
 *
 * TEORIA DE MÁQUINAS DE ESTADO (State Machine Parsing):
 * A variável 'tem_cabecalho' atua como o estado do parser. 
 * Estado 0: Procurando a linha de parâmetros do sistema.
 * Estado 1: Lendo as tarefas para a fila de prontos.
 * ==========================================================================*/
int config_carregar_texto(const char *conteudo, Estado *e, Diagnostico *d)
{
    char vazio[1] = "";
    char *copia, *linha, *prox;
    int num = 0;
    int tem_cabecalho = 0;
    int capacidade = 0;
    int valido = 1;
    /* Zera a memória do Estado inicial. Previne que "lixo" de RAM 
     * interfira na execução do escalonador. */
    memset(e, 0, sizeof *e);

    /* Aplica as definições base do sistema antes de começar a ler. */  
    strcpy(e->algoritmo, PADRAO_ALGORITMO);
    e->quantum = PADRAO_QUANTUM;
    e->ncpus = PADRAO_CPUS;
    /* 
     * Ficheiros de texto gravados no Windows (ex: Bloco de Notas) em UTF-8 
     * muitas vezes começam com 3 bytes invisíveis (0xEF, 0xBB, 0xBF). 
     * Se não ignorarmos isto, o parser vai ler "?algoritmo" em vez de "algoritmo", 
     * reprovando o ficheiro logo na primeira linha. */
    if (conteudo == NULL || *conteudo == '\0')
    {
        diag_erro(d, "arquivo vazio");
        return 0;
    }

    if (strncmp(conteudo, "\xEF\xBB\xBF", 3) == 0)
        conteudo += 3;

    /* 
     * O nosso parser altera a string original (substituindo ';' e '\n' por '\0'). 
     * Como 'conteudo' é 'const char *' (não pode ser modificado), 
     * criamos uma cópia exata na Heap para podermos mutilar à vontade. */
    copia = malloc(strlen(conteudo) + 1);

    if (copia == NULL)
    {
        diag_erro(d, "falta de memoria ao ler o arquivo");
        return 0;
    }

    strcpy(copia, conteudo);

    /* 
     * Em vez de ler linha a linha do disco (fgets), iteramos os ponteiros na RAM.
     * O 'strchr' procura o próximo '\n', corta-o com '\0', e guarda a morada 
     * do resto do texto no ponteiro 'prox'. */
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
        /* Ignora linhas vazias ou comentários (Req. 3.3.6) */
        if (*txt == '\0' || *txt == '#')
            continue;
        
        n = split_campos(txt, campos, MAX_CAMPOS);
        /* Limpa os espaços em branco isolados dentro de cada coluna específica. */
        for (i = 0; i < MAX_CAMPOS; i++)
            campos[i] = (i < n) ? trim(campos[i]) : vazio;

        /* ----------------------- ESTADO 0: CABEÇALHO ----------------------- */
        if (!tem_cabecalho)
        {
            /* primeira linha nao vazia/comentario = cabecalho, nao tarefa */
            tem_cabecalho = 1;

            /* Validação defensiva do tamanho da string do algoritmo para 
             * evitar Buffer Overflow na variável estática do Estado. */
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

        /* ------------------------- ESTADO 1: TAREFAS ----------------------- */
        memset(&t, 0, sizeof t);

        /* Indica que a tarefa começa sem estar alocada a nenhum processador. */
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
                /* 
                 * Em sistemas críticos clássicos, quando o prazo não é especificado, 
                 * assume-se que a tarefa deve terminar exatamente antes da sua 
                 * próxima ativação (prazo = período). */
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
            /* Proteção contra sobrecarga do buffer de eventos estático. */
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
        /* Trinco Lógico: se qualquer campo falhou nesta iteração, 
         * descarta a linha inteira mas prossegue para encontrar mais erros. */
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
        /* Insere o TCB válido no vetor do sistema. */
        e->tarefas[e->ntarefas++] = t;
    }

    free(copia);

    /* ---------------------- FINALIZAÇÃO E VALIDAÇÃO ----------------------- */

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
        /* calloc inicializa nativamente com zeros, prevenindo resíduos na RAM */
        e->cpus = calloc((size_t)e->ncpus, sizeof *e->cpus);

        if (e->cpus == NULL)
        {
            diag_erro(d, "falta de memoria para %d CPUs", e->ncpus);
            valido = 0;
        }
    }
    /* Se ocorreu qualquer erro, limpa a memória já alocada e aborta a carga. */
    if (!valido)
    {
        estado_liberar(e);
        return 0;
    }
    /* Prepara o estado inicial do Escalonador, marcando todos os 
     * núcleos como Ociosos (tarefa = -1). */
    for (int i = 0; i < e->ncpus; i++)
    {
        e->cpus[i].id = i;
        e->cpus[i].tarefa = -1;
    }

    return 1;
}