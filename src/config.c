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
    if (d->n_erros >= MAX_DIAG)
    {
        return;
    }
    va_list args;
    va_start(args,fmt);
    
    vsnprintf(d->erros[d->n_erros],MAX_DIAG_TXT,fmt, args);
    va_end(args);        
    d->n_erros++;
    
}

void diag_aviso(Diagnostico *d, const char *fmt, ...)
{
    /* TODO: igual a diag_erro, mas em d->avisos / d->n_avisos. */
    if (d->n_avisos >= MAX_DIAG)
    {
        return;
    }
    va_list args;
    va_start(args,fmt);
    
    vsnprintf(d->avisos[d->n_avisos],MAX_DIAG_TXT,fmt, args);
    va_end(args);        
    d->n_avisos++;
}

/* ------------------------------ utilitários ------------------------------ */

int str_igual_ci(const char *a, const char *b)
{
    /* TODO: comparar caractere a caractere ignorando maiúsculas/minúsculas
     * (tolower((unsigned char)*a) == tolower((unsigned char)*b)), até
     * achar diferença ou os dois chegarem no '\0' ao mesmo tempo.
     * Cuidado com a===NULL/b==NULL antes de desreferenciar. */
    //serve para comparar duas strings garantindo que "RM", "rm" ou "Rm" sejam considerados exatamente a mesma coisa.
    if (a == NULL || b == NULL)
    {
        return 0;
    }
    //Laço que funciona até as duas palavras termine e verifica se as letras são igauais
    while (*a != '\0' && *b != '\0' && tolower((unsigned char)*a) == tolower((unsigned char)*b))
    {
        a++;
        b++;
    }
    if (*a == '\0'&& *b=='\0')
    {
        return 1;
    }

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
    int len = strlen(s);
    
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0'; // Substitui o espaço por fim de string
        len--;             // Recua o tamanho
    }
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++; // Avança o ponteiro para a direita
    }
        
    
    return s;
}

static void str_maiuscula(char *s)
{
    /* TODO: percorrer a string e aplicar toupper((unsigned char)*s) em
     * cada posição, in place. */
    while (*s != '\0')
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
    //Essas duas variáveis servem como contrladores na leitura dos campos 
     int quant = 0;
    char *ini = linha;
    //Evita o estouro de memória do array
    while (quant < max)
    {
        //procura a primeira ocorrência do caractere e retorna um ponteiro para ele. Nesse caso, vai muda o ';' por '\0'
        char *pont_temp;
        pont_temp = strchr(ini, ';');
        if (pont_temp !=NULL)
        {
            campos[quant] = ini;
            *pont_temp = '\0';
            ini = pont_temp+1;
            quant++;

        }else
        {
            campos[quant] = ini;
            quant++;
            break; 
        }

        
    }
    if (quant > 0 && campos[quant - 1][0] == '\0')
    {
        quant--;
    }

    return quant;
}
//Conversor de texto para número
/* Lê um inteiro validando que o texto inteiro é numérico.
 * Campo vazio => devolve `padrao` (requisito 3.2).
 * Texto inválido => devolve 0 e sinaliza erro em *ok. */
static int campo_int(const char *txt, int padrao, int *ok)
{
    /* TODO: se txt for NULL/vazio, *ok=1 e devolve 'padrao'. Senão, usar
     * strtol(txt, &fim, 10) e conferir que 'fim' chegou no '\0' (ou seja,
     * a string inteira foi consumida, sem lixo depois do número) e que
     * fim != txt (realmente leu algo). Se não validar, *ok=0 e devolve 0. */
    if ( txt == NULL || txt[0] == '\0')
    {
        *ok = 1;
        return padrao;
    }
    char *fim; 
    long resultado = 0;
    resultado = strtol(txt,&fim,10);
    if (fim == txt || *fim!='\0')
    {
        *ok = 0;
        return 0; 
    }else
    {
        *ok = 1;
        return (int)resultado;
    }
    
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
    
    //se o atributo parametrizado for nulo, deve-se declara valores padrões para cada variável que armazena o valor da cor
    if (txt[0] == '\0')
    {
        t->r = PADRAO_COR_R;
        t->g = PADRAO_COR_G;
        t->b = PADRAO_COR_B;

        return 1;
    }else
    {
        
        const char *texto_cor = txt;
        //Analisando se o texto recebido no txt começava com # ou sem.
        if (texto_cor[0] == '#')
        {
            texto_cor++;
        }

        //Verificar quantidade de caracteres que texto_cor possui
        if (strlen(texto_cor) == 6)
        {
            for (int i = 0; i < strlen(texto_cor); i++)
            {
                if (!isxdigit(texto_cor[i])
                {
                    return 0;
                }
            }
            
        }else
        {
            return 0;
        }
        unsigned int vr,vg,vb;
        
        //sscanf(texto a ler lido, formato a ler, variáveis que receram o valor)
        sscanf(texto_cor,"%2x%2x%2x", &vr,&vg,&vb);

        t->r = vr;
        t->g = vg;
        t->b = vb;

        return 1;
    }

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
    //Passo 2: Procura o ficheiro que está neste caminho e abre-o no modo de leitura ("r" vem de read).
    int capacidade_atual = 0;
    int cabecalho_lido = 0;
    FILE *f = fopen(caminho, "r");
    if (f == NULL) {
        diag_erro(d, "Falha ao abrir o arquivo: %s", caminho);
        return 0; // Aborta se não conseguir abrir (ex: arquivo não existe)
    }

    strcpy(e->algoritmo,PADRAO_ALGORITMO);//armazenando valores para variável de string
    e->quantum = PADRAO_QUANTUM;
    e->ncpus = PADRAO_CPUS;
    e->tarefas = NULL;
    e->cpus = NULL;
    e->ntarefas = 0;

    char buf[MAX_LINHA];
    
    int num_linha = 0;
    while(fgets(buf,MAX_LINHA, f) != NULL)
    {
        num_linha++;
        
        char *linha_limpa = trim(buf);
        if (linha_limpa[0]=='\0')
        {
            continue;
        }
        char *campos[MAX_LINHA];
        //Representa quantos pedaços de texto a linha atual do arquivo tinha.
        int ncampos = split_campos(linha_limpa, campos,MAX_LINHA);
        //Retirar espaços vazios
        for (int i = 0; i < ncampos; i++)
        {
            campos[i] = trim(campos[i]);
        }
        

        //Precisa de uma flag para conseguir verificar qual linha ficará o cabeçalho
        if (!cabecalho_lido)
        {
            //Tratando  o campo[0]
            str_maiuscula(campos[0]);
            strncpy(e->algoritmo, campos[0],sizeof(e->algoritmo) - 1);
            e->algoritmo[sizeof(e->algoritmo) - 1] = '\0'; //Garantindo o fim de string

            //Trata o quantum e ncpus usando campo_int(passando valores padrão)
            int ok_quantum = 1,ok_cpus = 1;
            e->quantum = campo_int(campos[1],PADRAO_QUANTUM,&ok_quantum);
            e->ncpus = campo_int(campos[2], PADRAO_CPUS,&ok_cpus);
            
            if (e->ncpus < 1)
            {
                diag_erro(d, "Linha %d: quantidade de CPUs inválida", num_linha);
            }
            
            cabecalho_lido = 1;
            continue;
        }

        //Leitura é feita por linha, assim para pegar cada valor de variável estará armazenada em cada campo.
        int ok_id = 1,ok_prazo = 1,ok_duracao = 1, ok_ingresso = 1, ok_periodo=1;
        TCB tarefa;

        //Campo.int(valor do campo,valor default, endereço para verificação se o valor é válido)
        tarefa.id = campo_int(campos[0], 0,&ok_id);
        //Maneira da variável tarefa recebe o valor da cor
        campo_cor(campos[1], &tarefa);
        tarefa.ingresso = campo_int(campos[2],0,&ok_ingresso);
        tarefa.duracao = campo_int(campos[3],0,&ok_duracao);
        tarefa.periodo = campo_int(campos[4],0,&ok_periodo);
        
        //campo_int aciona o valor padrão (tarefa.periodo) automaticamente caso o campo no arquivo venha vazio.
        tarefa.prazo    = campo_int(campos[5], tarefa.periodo, &ok_prazo);

        // Cópia de texto bruta para os eventos (req 3.3.5 do seu projeto)
        strncpy(tarefa.eventos, campos[6], sizeof(tarefa.eventos) - 1);
        tarefa.eventos[sizeof(tarefa.eventos) - 1] = '\0'; // Prevenção de estouro de memória
        
        //Validação das flags para caso tenha recebido um valor inválido
        if (ok_id == 0 || ok_duracao == 0 || ok_ingresso == 0 || ok_periodo == 0 || ok_prazo == 0)
        {
            diag_erro(d, "Linha %d: Valores numericos invalidos", num_linha);
            continue;
        }
        //Verificação se o período > 0, pois se acontecer ao contrário a lógica do prgrama será comprometida
        
        if (tarefa.periodo == 0) {
            diag_aviso(d, "Linha %d: Periodo 0, ignorando tarefa.", num_linha);
            continue;
        } else if (tarefa.periodo < 0) {
            diag_erro(d, "Linha %d: Periodo negativo (invalido).", num_linha);
            continue;
        }


        //Inicializando os valores para a tarefa nova         
        tarefa.estado = EST_INATIVA;
        tarefa.exec_restante = 0;
        tarefa.cpu = -1;
        tarefa.ativacoes = 0;
        tarefa.sorteada = 0;
        tarefa.ativacao = 0;
        tarefa.deadline_abs = 0;

        if(e->ntarefas == capacidade_atual)
        {
            if(capacidade_atual == 0)
            {
                capacidade_atual = 2;
            }else if(capacidade_atual > 0)
            {
                capacidade_atual = capacidade_atual*2;
            }
            TCB *temporario = realloc(e->tarefas,capacidade_atual * sizeof(TCB));

            //Controle para evitar vazamento de memória
            if (temporario == NULL)
            {
                diag_erro(d,"Erro na linha %d: O espaço na memória RAM já está cheio", num_linha);
                break;
            }else
            {
                e->tarefas = temporario;
            }
 

        }
        //Armazenamento da tarefa
        e->tarefas[e->ntarefas] = tarefa;
        e->ntarefas++;
    
    }
    fclose(f);
    
    //Verificação se o arquivo estava vazio
    if ( cabecalho_lido == 0)
    {
        diag_erro(d, "O arquivo se encontra vazio");
    }
    
    //Alocar o processadores que serão utilizados na CPU
    e->cpus = malloc(e->ncpus*sizeof(CPU));

    if (e->cpus != NULL)
    {
        for (int i = 0; i < e->ncpus; i++)
        {
            e->cpus[i].id = i;
            e->cpus[i].tarefa = -1;
            e->cpus[i].ticks_desligada = 0;
        }   
    }
     
    e->tick = 0;

    if (d->n_erros == 0)
    {
        return 1;
    }else
    {
        return 0;
    }
    

}