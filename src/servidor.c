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
 * pedido veio quebrado. Quem chama dá free em p->corpo. */
static int ler_pedido(soquete_t c, Pedido *p)
{
    /* TODO: fica para a etapa 2. */
    (void)c;
    (void)p;
    return 0;
}

/* --------------------------- etapa 3: responder --------------------------- */

/* Manda uma resposta HTTP completa: a linha de status, os cabeçalhos
 * (Content-Type e Content-Length) e o corpo. */
static void responder(soquete_t c, const char *status, const char *tipo,
                      const void *corpo, size_t tam)
{
    /* TODO: fica para a etapa 3. */
    (void)c;
    (void)status;
    (void)tipo;
    (void)corpo;
    (void)tam;
}

/* Decide o que fazer com um pedido: servir um arquivo da página ou executar
 * uma das rotas /api. */
static void atender(Simulacao *sim, soquete_t c)
{
    /* TODO: fica para as etapas 3 e 4. */
    (void)sim;
    (void)c;
    (void)ler_pedido;
    (void)responder;
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
    /* TODO: montar o comando com snprintf e executar com system():
     *   Windows:  start "" "http://127.0.0.1:PORTA/"
     *   Linux:    xdg-open "http://127.0.0.1:PORTA/"
     *   Mac:      open "http://127.0.0.1:PORTA/"
     * (#ifdef _WIN32 / #elif defined(__APPLE__) / #else)
     * O "" depois do start é o título da janela: sem ele, o start entende o
     * endereço entre aspas como título e não abre nada. */
    (void)porta;
}
