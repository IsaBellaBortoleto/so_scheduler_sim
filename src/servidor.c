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
    size_t tam_corpo;             /* quantos bytes há em corpo                */
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
    char *fim;      /* onde o cabeçalho termina e o corpo começa            */
    char *q;        /* posição do '?' no caminho                            */
    int n = 0;      /* quantos bytes já estão em 'cab'                      */
   

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
        if (strstr(cab, "\r\n\r\n") != NULL)
            break; /* achou o fim do cabeçalho */
    }
    fim=strstr(cab, "\r\n\r\n");
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

    /* FASES C e D: o corpo. Só o POST tem, e o cabeçalho Content-Length diz
     * quantos bytes são. Sem corpo, p->corpo fica NULL (free(NULL) é seguro). */
    p->corpo = NULL;
    p->tam_corpo = 0;
    if (strcmp(p->metodo, "POST") == 0)
    {
        const char *content_length_str = strstr(cab, "Content-Length:");
        if (content_length_str != NULL)
        {
            /* + 15 pula o texto "Content-Length:"; o atoi lê o número. */
            int content_length = atoi(content_length_str + 15);
            /* Teto de 1 MB: o número vem de fora e vai para o malloc. O
             * arquivo de configuração tem poucos KB. */
            if (content_length > 1024*1024)
                return 0;
           if(content_length > 0)
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
                p->tam_corpo = content_length;
            }
        }
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
 * (Content-Type e Content-Length) e o corpo. */
static void responder(soquete_t c, const char *status, const char *tipo,
                      const void *corpo, size_t tam)
{
    char cab[256];
    int n = snprintf(cab, sizeof cab,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, tipo, (unsigned long)tam);

    enviar_tudo(c, cab, (size_t)n); /* cabeçalho primeiro */
    if (tam > 0)
        enviar_tudo(c, corpo, tam); /* depois o corpo, se houver */
}

/* Resposta curta de texto (erros 400, 404, 500). O strlen conta o tamanho:
 * contar os caracteres à mão é fácil de errar. */
static void responder_texto(soquete_t c, const char *status, const char *msg)
{
    responder(c, status, "text/plain; charset=utf-8", msg, strlen(msg));
}

/* ---------------------------- etapa 4: as rotas --------------------------- */

/* Onde o JSON das respostas é montado. 256 KB: o estado de ~190 tarefas (o
 * porte do arquivo do professor) ocupa uns 60 KB. É 'static' para não ficar
 * na pilha, e pode ser um só porque o servidor atende um pedido por vez. */
static char json[262144];

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
            responder(c, "200 OK", r->tipo_mime, r->dados, r->tamanho);
            return 1;
        }
    }
    return 0; /* olhou a tabela INTEIRA e não achou */
}

/* Manda o estado atual da simulação em JSON. É a resposta de quase todas as
 * rotas /api: a página sempre recebe o estado novo e se redesenha.
 * 'd' traz os avisos a mandar junto; NULL = sem avisos (todas as rotas,
 * menos a de carregar). */
static void responder_estado(Simulacao *sim, soquete_t c, const Diagnostico *d)
{
    int n = estado_json_com_avisos(sim, d, json, sizeof json);
    if (n < 0)
        responder_texto(c, "500 Internal Server Error", "estado grande demais");
    else
        responder(c, "200 OK", "application/json; charset=utf-8", json, (size_t)n);
}

/* POST /api/carregar: o corpo do pedido é o CONTEÚDO do arquivo escolhido na
 * página (por isso funciona do pendrive: o C nunca abre caminho nenhum).
 * Deu certo -> responde o estado novo, com os avisos. Deu errado -> responde
 * a lista de erros, e a simulação anterior continua valendo. */
static void rota_carregar(Simulacao *sim, soquete_t c, const Pedido *p)
{
    static Diagnostico d; /* static: tem uns 25 KB */
    Estado ini;
    int ok = 0;
    int n;

    memset(&d, 0, sizeof d);
    if (p->corpo == NULL)
        diag_erro(&d, "nenhum arquivo foi enviado");
    else
        ok = config_carregar_texto(p->corpo, &ini, &d);

    if (ok)
    {
        /* TODO: chamar sim_liberar(sim) quando ela existir; hoje o histórico
         * da simulação anterior fica sem ser liberado. */
        ok = sim_iniciar(sim, &ini, &d);
        estado_liberar(&ini); /* sim_iniciar guardou uma cópia */
    }
    /* Se config_carregar_texto falhou, ela mesma já liberou 'ini'. */

    if (ok)
    {
        /* &d: os avisos do carregamento vão junto com o estado. */
        responder_estado(sim, c, &d);
        return;
    }
    /* Falhou: {"erro":true,"erros":[...],"avisos":[...]}. O status é 200
     * porque o pedido HTTP funcionou; quem falhou foi o arquivo, e a página
     * sabe disso pelo campo "erro". */
    n = diagnostico_json(&d, json, sizeof json);
    if (n < 0)
        responder_texto(c, "500 Internal Server Error", "erros demais para listar");
    else
        responder(c, "200 OK", "application/json; charset=utf-8", json, (size_t)n);
}

/* O ROTEADOR: lê um pedido e escolhe quem responde. Não faz o trabalho, só
 * decide; cada rota tem a sua função, logo acima.
 *
 *   GET  /, /app.js, /estilo.css   -> servir_recurso    (a página)
 *   GET  /api/estado               -> responder_estado  (o estado em JSON)
 *   POST /api/carregar             -> rota_carregar     (recebe o arquivo)
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
    get = strcmp(p.metodo, "GET") == 0;
    post = strcmp(p.metodo, "POST") == 0;

    /* As rotas, em ordem. A primeira que reconhecer o pedido responde; se
     * nenhuma reconhecer, é 404. Cada rota confere MÉTODO e CAMINHO. */
    if (get && servir_recurso(c, &p))
    {
        /* era um arquivo da página; servir_recurso já respondeu */
    }
    else if (get && strcmp(p.caminho, "/api/estado") == 0)
        responder_estado(sim, c, NULL);
    else if (post && strcmp(p.caminho, "/api/carregar") == 0)
        rota_carregar(sim, c, &p);
    else
        responder_texto(c, "404 Not Found", "rota inexistente");

    free(p.corpo); /* um só free, no fim: vale para todas as rotas */
}

/* ------------------------------ API pública ------------------------------ */

int servidor_iniciar(Simulacao *sim, int porta)
{
    /* São DOIS sockets: 's' é o da porta, que só recebe conexões e vive o
     * programa inteiro; 'c' é o de UMA conversa com o navegador, criado pelo
     * accept e fechado depois de responder. */
    soquete_t s = abrir_porta(porta);
    if (s == SOQUETE_INVALIDO)
        return 0; /* ex.: porta já em uso; o main avisa o usuário */

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
        atender(sim, c);
        fechar_soquete(c);
    }
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
