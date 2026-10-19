/* ============================================================================
 * servidor.c — servidor HTTP embutido no executável
 *
 * RESUMO
 *  - O navegador é só a tela. Ele conversa com este arquivo por HTTP: manda
 *    um pedido (texto) e recebe uma resposta (texto).
 *  - Usa apenas SOCKETS, que são do sistema operacional: Winsock no Windows,
 *    BSD sockets no Linux/Mac. Nenhuma biblioteca a instalar (req. 5).
 *  - Escuta só em 127.0.0.1, a própria máquina: ninguém da rede alcança o
 *    simulador, e o Windows não mostra o alerta do firewall.
 *  - Atende UM pedido por vez, em laço. Basta: só existe um usuário.
 *  - Não guarda estado próprio: cada pedido lê ou comanda a Simulacao que o
 *    main mantém viva.
 *
 * CAMINHO DE UM PEDIDO
 *   abrir_porta -> [laço] accept -> ler_pedido -> atender -> responder -> fecha
 * ==========================================================================*/
#include "servidor.h"
#include "estado_json.h"
#include "gantt.h"
#include "recursos_web.h" /* a página, embutida pelo gerar_recursos */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------- diferenças entre Windows e Linux/Mac ----------------- */
/* As funções de socket têm os mesmos nomes nos dois (socket, bind, listen,
 * accept, recv, send). Mudam só três coisas, resolvidas aqui uma vez:
 * o cabeçalho, o tipo do socket e o nome da função que fecha. */
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
typedef SOCKET soquete_t;
#  define SOQUETE_INVALIDO INVALID_SOCKET
#  define fechar_soquete(s) closesocket(s)
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/time.h>   /* struct timeval, do tempo-limite de leitura */
#  include <unistd.h>
typedef int soquete_t;
#  define SOQUETE_INVALIDO (-1)
#  define fechar_soquete(s) close(s)
#endif

/* ------------------------------- o pedido -------------------------------- */

#define MAX_CAMINHO 256

/* O que interessa de um pedido HTTP, já separado. */
typedef struct {
    char   metodo[8];             /* "GET" ou "POST"                          */
    char   caminho[MAX_CAMINHO];  /* ex. "/" ou "/api/carregar"               */
    char  *corpo;                 /* dados do POST (malloc), com '\0' no fim  */
    size_t tam_corpo;   /* quantos bytes chegaram; o corpo pode ter '\0' no meio */
} Pedido;

/* ------------------------------ etapa 1: porta ---------------------------- */

/* Abre a porta e deixa o socket pronto para receber conexões.
 * Devolve SOQUETE_INVALIDO se não conseguir (ex. porta já em uso). */
static soquete_t abrir_porta(int porta)
{
    struct sockaddr_in endereco;
    soquete_t s;

#ifdef _WIN32
    /* No Windows a Winsock precisa ser "ligada" antes de qualquer socket. */
    WSADATA dados;
    if (WSAStartup(MAKEWORD(2, 2), &dados) != 0)
        return SOQUETE_INVALIDO;
#endif

    /* AF_INET = IPv4, SOCK_STREAM = TCP (o que o HTTP usa). */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOQUETE_INVALIDO)
        return SOQUETE_INVALIDO;

    /* Endereço: a porta pedida, só em 127.0.0.1 (INADDR_LOOPBACK). htons e
     * htonl põem os bytes na ordem que a rede usa. */
    memset(&endereco, 0, sizeof endereco);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons((unsigned short)porta);
    endereco.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /* bind = "esta porta é minha" (falha se outro programa já a usa);
     * listen = "pode vir conexão", com até 8 esperando na fila. O || só
     * chama o listen se o bind deu certo. */
    if (bind(s, (struct sockaddr *)&endereco, sizeof endereco) != 0 ||
        listen(s, 8) != 0)
    {
        fechar_soquete(s);
        return SOQUETE_INVALIDO;
    }
    return s;
}

/* ------------------------- etapa 2: ler o pedido ------------------------- */

/* Lê um pedido HTTP do socket e preenche 'p'. Devolve 1 se leu, 0 se o
 * pedido veio quebrado. Quem chama dá free em p->corpo.
 *
 * Um pedido HTTP é TEXTO, com esta forma:
 *     POST /api/carregar HTTP/1.1      <- método e caminho
 *     Content-Length: 42               <- cabeçalhos, um por linha
 *                                      <- linha em branco = fim do cabeçalho
 *     RM;2;2 ...                       <- corpo (só no POST)
 * Cada linha termina em \r\n; por isso a linha em branco é "\r\n\r\n".
 *
 * QUATRO FASES
 *   A) lê até a linha em branco        C) descobre o tamanho do corpo
 *   B) separa método e caminho         D) lê o corpo */
static int ler_pedido(soquete_t c, Pedido *p)
{
    char cab[8192]; /* o cabeçalho; 8 KB sobra para o que o navegador manda */
    char *fim = NULL; /* onde o cabeçalho termina e o corpo começa          */
    char *q;          /* posição do '?' no caminho                          */
    const char *cl;   /* posição do cabeçalho Content-Length                */
    int content_length;
    int n = 0;        /* quantos bytes já estão em 'cab'                    */

    /* FASE A: ler até a linha em branco. O recv entrega os bytes em pedaços,
     * sem garantia de vir tudo de uma vez; por isso o laço, que vai juntando
     * em 'cab'. O "- 1" guarda lugar para o '\0'. */
    while (n < (int)sizeof cab - 1)
    {
        int lidos = recv(c, cab + n, sizeof cab - 1 - n, 0);
        if (lidos <= 0)
            return 0; /* conexão fechada ou erro */
        n += lidos;
        cab[n] = '\0'; /* vira texto: dá para usar strstr e sscanf */
        if ((fim = strstr(cab, "\r\n\r\n")) != NULL)
            break; /* achou o fim do cabeçalho */
    }
    if (fim == NULL)
        return 0; /* cabeçalho muito grande ou mal formado */   
    fim += 4; /* pula os 4 caracteres de "\r\n\r\n": aqui começa o corpo */

    /* FASE B: as duas primeiras palavras são o método e o caminho. Os limites
     * 7 e 255 são o tamanho dos campos menos 1, para o sscanf não escrever
     * fora deles. */
    if(sscanf(cab, "%7s %255s", p->metodo, p->caminho) != 2)
        return 0; /* pedido mal formado */
    q=strchr(p->caminho, '?');
    if(q != NULL)
        *q='\0'; /* corta no '?': "/api/gantt?v=3" vira "/api/gantt" */

    /* FASES C e D: o corpo. O cabeçalho Content-Length diz quantos bytes são;
     * sem ele (o GET não manda), o tamanho é 0 e p->corpo fica NULL
     * (free(NULL) é seguro). + 15 pula o texto "Content-Length:". */
    p->corpo = NULL;
    p->tam_corpo = 0;
    cl = strstr(cab, "Content-Length:");
    content_length = cl ? atoi(cl + 15) : 0;
    /* Teto de 1 MB: o número vem de fora e vai para o malloc. O
     * arquivo de configuração tem poucos KB. */
    if (content_length > 1024*1024)
        return 0;
    if (content_length > 0)
    {
        /* O recv da fase A não para na linha em branco: parte do
         * corpo (ou ele todo) pode já estar em 'cab'. 'ja' é quantos
         * bytes do corpo já chegaram. Eles são copiados, e só o que
         * FALTA é pedido ao socket. Pedir tudo de novo travaria o
         * servidor, esperando bytes que já foram lidos. */
        int ja=n-(int)(fim-cab);
        int total_lidos;
        if(ja > content_length)
            ja=content_length;

        p->corpo = malloc(content_length + 1); /* + 1 para o '\0' */
        if (p->corpo == NULL)
            return 0; /* falha de memória */
        memcpy(p->corpo, fim, ja); /* a parte que veio com o cabeçalho */
        total_lidos = ja;
        while (total_lidos < content_length)
        {
            int lidos = recv(c, p->corpo + total_lidos, content_length - total_lidos, 0);
            if (lidos <= 0)
            {
                free(p->corpo);
                return 0; /* conexão fechada ou erro */
            }
            total_lidos += lidos;
        }
        /* O parser trata o arquivo como texto, e texto em C precisa
         * terminar em '\0'. */
        p->corpo[content_length] = '\0';
        p->tam_corpo = (size_t)content_length;
    }
    return 1;
}

/* --------------------------- etapa 3: responder --------------------------- */

/* Envia 'tam' bytes a partir de 'dados'. O send pode mandar só uma parte
 * por chamada; o laço insiste até sair tudo. */
static void enviar_tudo(soquete_t c, const char *dados, size_t tam)
{
    size_t enviados = 0;
    while (enviados < tam)
    {
        int n = send(c, dados + enviados, tam - enviados, 0);
        if (n <= 0)
            return; /* conexão fechada ou erro */
        enviados += (size_t)n;
    }
}

/* Manda uma resposta HTTP completa: a linha de status, os cabeçalhos
 * (Content-Type e Content-Length) e o corpo. 'extra' é um cabeçalho a mais,
 * já com o \r\n no fim, ou "" se não houver: a exportação do Gantt usa
 * para mandar o Content-Disposition, que faz o navegador BAIXAR o arquivo. */
static void responder(soquete_t c, const char *status, const char *tipo,
                      const char *extra, const void *corpo, size_t tam)
{
    char cab[512];
    int n = snprintf(cab, sizeof cab,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lu\r\n"
             "%s"
             "Connection: close\r\n"
             "\r\n",
             status, tipo, (unsigned long)tam, extra);

    enviar_tudo(c, cab, (size_t)n); /* cabeçalho primeiro */
    if (tam > 0)
        enviar_tudo(c, corpo, tam); /* depois o corpo, se houver */
}

/* Resposta curta de texto (erros 400, 404, 500). O strlen conta o tamanho:
 * contar os caracteres à mão é fácil de errar. */
static void responder_texto(soquete_t c, const char *status, const char *msg)
{
    responder(c, status, "text/plain; charset=utf-8", "", msg, strlen(msg));
}

/* ---------------------------- etapa 4: as rotas --------------------------- */

/* Onde o JSON das respostas é montado. 256 KB: o estado de ~190 tarefas (o
 * porte do arquivo do professor) ocupa uns 60 KB. É 'static' para não ficar
 * na pilha, e pode ser um só porque o servidor atende um pedido por vez. */
static char json[262144];

/* Erros e avisos das rotas que podem recusar o pedido. Pelo mesmo motivo
 * do 'json' é um só, 'static' (tem uns 25 KB); atender() zera a cada pedido. */
static Diagnostico diag;

/* Arquivos da página (/, /app.js, /estilo.css). Eles estão DENTRO do
 * executável, na tabela RECURSOS_WEB de recursos_web.h. Procura o caminho
 * pedido na tabela; devolve 1 se achou e respondeu, 0 se não é da página. */
static int servir_recurso(soquete_t c, const Pedido *p)
{
    int i;
    for (i = 0; i < RECURSOS_WEB_QTDE; i++)
    {
        const RecursoWeb *r = &RECURSOS_WEB[i];
        if (strcmp(p->caminho, r->caminho) == 0)
        {
            responder(c, "200 OK", r->tipo_mime, "", r->dados, r->tamanho);
            return 1;
        }
    }
    return 0; /* olhou a tabela INTEIRA e não achou */
}

/* Manda os 'n' bytes já montados em 'json'; n < 0 = não coube, e vai o
 * 500 com a mensagem 'erro'. */
static void responder_json(soquete_t c, int n, const char *erro)
{
    if (n < 0)
        responder_texto(c, "500 Internal Server Error", erro);
    else
        responder(c, "200 OK", "application/json; charset=utf-8", "", json, (size_t)n);
}

/* Manda o estado atual da simulação em JSON. É a resposta de quase todas as
 * rotas /api: a página sempre recebe o estado novo e se redesenha.
 * 'd' traz os avisos a mandar junto; NULL = sem avisos (todas as rotas,
 * menos a de carregar). */
static void responder_estado(Simulacao *sim, soquete_t c, const Diagnostico *d)
{
    responder_json(c, estado_json_com_avisos(sim, d, json, sizeof json),
                   "estado grande demais");
}

/* Manda a lista de erros: {"erro":true,"erros":[...],"avisos":[...]}. O
 * status é 200 porque o pedido HTTP funcionou; quem falhou foi o arquivo ou
 * a edição, e a página sabe disso pelo campo "erro". */
static void responder_erros(soquete_t c, const Diagnostico *d)
{
    responder_json(c, diagnostico_json(d, json, sizeof json), "erros demais para listar");
}
/* Arquivo de texto só tem caracteres visíveis e as quebras de linha
 * (\n, \r, \t). Byte zero ou outro caractere de controle (< 0x20) é sinal
 * de arquivo binário: imagem, executável... (checklist da defesa).
 * Bytes >= 0x80 passam: são o BOM e as letras acentuadas do UTF-8. */
static int parece_texto(const char *dados, size_t tam)
{
    for (size_t k = 0; k < tam; k++)
    {
        unsigned char b = (unsigned char)dados[k];
        if (b < 0x20 && b != '\n' && b != '\r' && b != '\t')
            return 0;
    }
    return 1;
}
/* POST /api/carregar: o corpo do pedido é o CONTEÚDO do arquivo escolhido na
 * página (por isso funciona do pendrive: o C nunca abre caminho nenhum).
 * Deu certo -> responde o estado novo, com os avisos. Deu errado -> responde
 * a lista de erros, e a simulação anterior continua valendo. */
static void rota_carregar(Simulacao *sim, soquete_t c, const Pedido *p)
{
    Estado ini;
    int ok = 0;

    if (p->corpo == NULL)
        diag_erro(&diag, "nenhum arquivo foi enviado");
    else if (!parece_texto(p->corpo, p->tam_corpo))
        diag_erro(&diag, "o arquivo escolhido nao e um arquivo de texto; "
                         "escolha o .txt de configuracao");
    else
        ok = config_carregar_texto(p->corpo, &ini, &diag);

    if (ok)
    {
        /* A memória da simulação anterior é liberada dentro de
         * sim_iniciar, e só depois de o arquivo novo ser aceito. */
        ok = sim_iniciar(sim, &ini, &diag);
        estado_liberar(&ini); /* sim_iniciar guardou uma cópia */
    }
    /* Se config_carregar_texto falhou, ela mesma já liberou 'ini'. */

    if (ok)
    {
        /* &diag: os avisos do carregamento vão junto com o estado. */
        responder_estado(sim, c, &diag);
        return;
    }
    responder_erros(c, &diag); /* falhou: a simulação anterior continua */
}

/* POST /api/editar (req. 1.5.2 e 3.4): muda o estado ou um campo de uma
 * tarefa no meio da simulação. Quem VALIDA é o núcleo (sim_editar_tarefa);
 * esta rota só lê o pedido e devolve a resposta.
 * Aceita -> responde o estado novo. Recusada -> responde o motivo no mesmo
 * formato de erro do carregamento: {"erro":true,"erros":["motivo"],...}. */
static void rota_editar(Simulacao *sim, soquete_t c, const Pedido *p)
{
    int id;
    char campo[32], valor[64];
    char motivo[MAX_DIAG_TXT];  /* sim_editar_tarefa escreve aqui o porquê */

    /* O corpo chega como {"id":3,"campo":"estado","valor":"pronta"}.
     * %31[^"] lê tudo até a próxima aspa, no máximo 31 caracteres (o
     * tamanho do vetor menos 1). O sscanf devolve quantos campos leu: menos
     * de 3 é pedido quebrado.
     * só lê o JSON exatamente nesta ordem e sem espaços, que é o
     * que o JSON.stringify da página gera. Outro cliente precisaria de um
     * leitor de JSON de verdade. */
    if (p->corpo == NULL ||
        sscanf(p->corpo, "{\"id\":%d,\"campo\":\"%31[^\"]\",\"valor\":\"%63[^\"]\"}",
               &id, campo, valor) != 3)
    {
        diag_erro(&diag, "pedido de edicao mal formado");
    }
    else if (sim_editar_tarefa(sim, id, campo, valor, motivo, sizeof motivo))
    {
        /* Aceita: a página recebe o estado novo e se redesenha. */
        responder_estado(sim, c, NULL);
        return;
    }
    else
    {
        /* "%s" e não o motivo direto como formato: ele contém texto do
         * usuário, e um '%' digitado seria lido como formato do printf. */
        diag_erro(&diag, "%s", motivo);
    }
    responder_erros(c, &diag);
}

/* POST /api/algoritmo (req. 3.1): troca o algoritmo no meio da simulação.
 * O corpo é só o nome ("EDF"). Quem valida é o núcleo. */
static void rota_algoritmo(Simulacao *sim, soquete_t c, const Pedido *p)
{
    char motivo[MAX_DIAG_TXT];

    if (p->corpo == NULL)
        diag_erro(&diag, "nenhum algoritmo informado");
    else if (sim_trocar_algoritmo(sim, p->corpo, motivo, sizeof motivo))
    {
        responder_estado(sim, c, NULL);
        return;
    }
    else
        diag_erro(&diag, "%s", motivo);   /* "%s": o motivo traz texto do usuário */
    responder_erros(c, &diag);
}

/* GET /api/gantt e /api/exportar_svg (req. 2.3 e 2.4): o gráfico de Gantt
 * em SVG, desenhado pelo gantt.c. 'inteira' = 0 -> a janela da tela, os
 * últimos ticks (pelo menos GANTT_JANELA_TELA); 1 -> a simulação toda, como
 * download.
 *
 * O gantt_svg escreve num FILE*, e um socket não é FILE* no Windows. Então
 * o SVG vai para um arquivo em %TEMP%, é lido de volta e enviado. Não é
 * tmpfile(): no Windows ele tenta criar em C:\ e falha (ver gantt.h).
 * sem TEMP (comum no Linux) usa a pasta atual. */
static void responder_svg(soquete_t c, const Simulacao *sim, int inteira)
{
    char caminho[512];
    const char *pasta = getenv("TEMP");
    int nt = sim->atual.ntarefas;
    int nc = sim->atual.ncpus;
    int janela = GANTT_JANELA_TELA;
    int fim = sim->n_hist - 1; /* a última foto é o tick atual */
    int ini;
    FILE *f;
    char *svg = NULL;
    long tam = -1;
    Simulacao copia;
    const Simulacao *desenho = sim; /* qual simulação vai para o desenho */
    int ok;

    /* Janela da tela: ~20 mil células por imagem. Arquivo pequeno cabe
     * inteiro; o do professor (~190 tarefas) fica com uns 100 ticks.
     * GANTT_JANELA_TELA é o mínimo. Sem simulação, nt + nc = 0: a conta só
     * é feita com linhas, senão seria divisão por zero (derruba o programa).
     * 20 mil é estimativa; medir no teste com 200 tarefas. */
    if (nt + nc > 0 && 20000 / (nt + nc) > janela)
        janela = 20000 / (nt + nc);
    /* Só DEPOIS de calcular a janela: calculado antes, usaria o mínimo. */
    ini = inteira ? 0 : fim - janela + 1;
    if (ini < 0)
        ini = 0;
    snprintf(caminho, sizeof caminho, "%s/simulador_gantt.svg", pasta ? pasta : ".");
    f = fopen(caminho, "w+b"); /* w+: escreve e depois lê o mesmo arquivo */
    if (f == NULL)
    {
        responder_texto(c, "500 Internal Server Error", "nao foi possivel criar o arquivo temporario");
        return;
    }
    /* Req. 2.4: exportar no meio da simulação dá a simulação INTEIRA. Uma
     * cópia é levada até o fim e desenhada; a da tela não muda. */
    if (inteira && !sim->terminada && sim->n_hist > 0)
    {
        if (!sim_copiar_ate_o_fim(sim, &copia))
        {
            fclose(f);
            remove(caminho);
            responder_texto(c, "500 Internal Server Error", "falta de memoria para exportar");
            return;
        }
        desenho = &copia;
        fim = copia.n_hist - 1; /* o fim da cópia, não o tick da tela */
    }
    ok = gantt_svg(desenho, f, ini, fim);
    if (desenho == &copia)
        sim_liberar_copia(sim, &copia); /* o SVG já está no arquivo */
    if (!ok)
    {
        fclose(f);
        remove(caminho);
        /* 409 = conflito com o estado atual: a rota existe, só não há
         * simulação carregada para desenhar. */
        responder_texto(c, "409 Conflict", "nenhuma simulacao carregada");
        return;
    }
    /* Tamanho = posição final da escrita; volta ao início e lê tudo. */
    tam = ftell(f);
    rewind(f);
    if (tam >= 0)
        svg = malloc(tam > 0 ? (size_t)tam : 1);
    if (svg != NULL && fread(svg, 1, (size_t)tam, f) == (size_t)tam)
        responder(c, "200 OK", "image/svg+xml; charset=utf-8",
                  inteira ? "Content-Disposition: attachment; filename=\"gantt.svg\"\r\n" : "",
                  svg, (size_t)tam);
    else
        responder_texto(c, "500 Internal Server Error", "falha ao ler o SVG gerado");
    free(svg);
    fclose(f);
    remove(caminho);
}

/* O ROTEADOR: lê um pedido e escolhe quem responde. Não faz o trabalho, só
 * decide; cada rota tem a sua função, logo acima.
 *
 *   GET  /, /app.js, /estilo.css   -> servir_recurso    (a página)
 *   GET  /api/estado               -> responder_estado  (o estado em JSON)
 *   POST /api/carregar             -> rota_carregar     (recebe o arquivo)
 *   POST /api/avancar              -> sim_avancar       (um tick)
 *   POST /api/retroceder           -> sim_retroceder    (volta um tick)
 *   POST /api/executar_tudo        -> sim_executar_tudo (até o fim)
 *   POST /api/editar               -> rota_editar       (muda uma tarefa)
 *   POST /api/algoritmo            -> rota_algoritmo    (troca o algoritmo)
 *   GET  /api/gantt                -> responder_svg     (janela da tela)
 *   GET  /api/exportar_svg         -> responder_svg     (simulação inteira)
 *   qualquer outra coisa           -> 404
 *
 * Rota nova = uma função nova e mais um "else if" aqui. */
static void atender(Simulacao *sim, soquete_t c)
{
    Pedido p;
    int get, post;

    if (!ler_pedido(c, &p))
    {
        /* Pedido quebrado ou grande demais. Responde mesmo assim: sem isto o
         * navegador só veria a conexão cair, sem saber o motivo. */
        responder_texto(c, "400 Bad Request", "pedido invalido");
        return; /* sem free: em caso de falha a ler_pedido já liberou */
    }
    memset(&diag, 0, sizeof diag);
    get = strcmp(p.metodo, "GET") == 0;
    post = strcmp(p.metodo, "POST") == 0;

    /* As rotas, em ordem. A primeira que reconhecer o pedido responde; se
     * nenhuma reconhecer, é 404. Cada rota confere MÉTODO e CAMINHO. */
    if (get && servir_recurso(c, &p))
    {
        /* era um arquivo da página; servir_recurso já respondeu */
    }
    else if (get && strcmp(p.caminho, "/api/estado") == 0)
    {
        responder_estado(sim, c, NULL);
    }else if (post && strcmp(p.caminho, "/api/carregar") == 0)
    {
        rota_carregar(sim, c, &p);
    }else if (post && strcmp(p.caminho, "/api/avancar") == 0)
    {
        sim_avancar(sim); // Executa um tick do relógio
        responder_estado(sim, c, NULL); // Devolve o estado atualizado para a interface
    }
    else if (post && strcmp(p.caminho, "/api/retroceder") == 0)
    {
        sim_retroceder(sim); // Volta no tempo
        responder_estado(sim, c, NULL); // Devolve o estado atualizado para a interface
    }
    else if (post && strcmp(p.caminho, "/api/executar_tudo") == 0)
    {
        sim_executar_tudo(sim); // Roda a simulação inteira de uma vez
        responder_estado(sim, c, NULL); // Devolve o resultado final para a interface
    }else if (post && strcmp(p.caminho,"/api/editar") == 0)
    {
        rota_editar(sim, c, &p);
    }
    else if (post && strcmp(p.caminho, "/api/algoritmo") == 0)
    {
        rota_algoritmo(sim, c, &p);
    }
    else if (get && strcmp(p.caminho, "/api/gantt") == 0)
    {
        responder_svg(c, sim, 0);
    }
    else if (get && strcmp(p.caminho, "/api/exportar_svg") == 0)
    {
        responder_svg(c, sim, 1);
    }
    else
    {
        responder_texto(c, "404 Not Found", "rota inexistente");
    }
        

    free(p.corpo); /* um só free, no fim: vale para todas as rotas */
}

/* ------------------------------ API pública ------------------------------ */

/* Tempo-limite de leitura de UMA conversa (card 5.6). O servidor atende um
 * pedido por vez; sem isto, uma conexão que abre e não manda nada deixaria
 * o recv da ler_pedido esperando para sempre, e ninguém mais seria atendido.
 * Navegadores fazem isso de propósito: abrem conexões "de reserva" para o
 * próximo clique. Com o limite, o recv desiste, a ler_pedido devolve 0 e a
 * conexão é fechada.
 * 500 ms bastam aqui, porque o navegador está na mesma máquina e
 * manda o pedido inteiro na hora; uma conexão de reserva atrasa os outros
 * pedidos em até meio segundo. Em rede de verdade, o certo seria atender
 * várias conexões ao mesmo tempo (select ou threads). */
static void limitar_espera(soquete_t c)
{
#ifdef _WIN32
    DWORD ms = 500; /* no Windows o valor é em milissegundos */
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof ms);
#else
    struct timeval tv = { 0, 500000 }; /* no Linux/Mac: segundos e microssegundos */
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
}

void servidor_abrir_navegador(int porta)
{
    /* Abrir o navegador é pedir isso ao sistema operacional, com o mesmo
     * comando que se digitaria no terminal. O system() executa um comando de
     * terminal; aqui só montamos o texto dele. Nenhuma biblioteca a mais.
     *
     * Cada sistema tem o seu comando, escolhido na COMPILAÇÃO pelo #ifdef:
     *   Windows:  start "" "http://127.0.0.1:8080/"
     *   Mac:      open "http://127.0.0.1:8080/"
     *   Linux:    xdg-open "http://127.0.0.1:8080/"
     *
     * O "" depois do start é o título da janela: sem ele, o start entende o
     * endereço entre aspas como título e não abre nada.
     * No código, \" é uma aspa DENTRO do texto, e %d recebe a porta. */
    char cmd[128];
#ifdef _WIN32
    snprintf(cmd, sizeof cmd, "start \"\" \"http://127.0.0.1:%d/\"", porta);
#elif defined(__APPLE__)
    snprintf(cmd, sizeof cmd, "open \"http://127.0.0.1:%d/\"", porta);
#else
    snprintf(cmd, sizeof cmd, "xdg-open \"http://127.0.0.1:%d/\"", porta);
#endif

    /* Sem tratamento de erro: se o navegador não abrir, o endereço já está
     * impresso no terminal (main.c) e o usuário pode abrir à mão. */
    system(cmd);
}

int servidor_iniciar(Simulacao *sim, int porta)
{
    /* São DOIS sockets: 's' é o da porta, que só recebe conexões e vive o
     * programa inteiro; 'c' é o de UMA conversa com o navegador, criado pelo
     * accept e fechado depois de responder. */
    soquete_t s = abrir_porta(porta);
    if (s == SOQUETE_INVALIDO)
        return 0; /* ex.: porta já em uso; o main avisa o usuário */

    /* A porta abriu: só agora é verdade que esta janela é o simulador. Se
     * o navegador não abrir sozinho (antivírus, navegador padrão não
     * configurado), esta linha é a única forma de o usuário descobrir onde
     * entrar. O fflush garante que ela apareça antes de o laço prender o
     * programa. */
    printf("Simulador em http://127.0.0.1:%d/\n", porta);
    printf("Deixe esta janela aberta; feche-a para encerrar.\n");
    fflush(stdout);

    /* Só AGORA, com a porta já escutando: aberto antes, o navegador poderia
     * pedir a página cedo demais e mostrar "conexão recusada". */
    servidor_abrir_navegador(porta);

    /* Um pedido por vez. O accept fica parado esperando alguém conectar; o
     * laço não tem saída e só termina quando o programa é fechado. */
    for (;;)
    {
        soquete_t c = accept(s, NULL, NULL);
        if (c == SOQUETE_INVALIDO)
            continue;
        limitar_espera(c);
        atender(sim, c);
        fechar_soquete(c);
    }
}
