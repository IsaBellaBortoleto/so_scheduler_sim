#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "kernel.h"

/* Servidor HTTP embutido (so sockets: Winsock no Windows, BSD sockets no
 * Linux/Mac -- nenhuma biblioteca externa) que expoe a Simulacao para a
 * pagina web. O servidor nao guarda estado proprio: cada requisicao le ou
 * comanda a MESMA struct Simulacao que o main.c mantem viva.
 *
 * ROTAS (o formato dos JSON esta no topo de web/app.js):
 *
 *   GET  /, /app.js, /estilo.css   pagina, servida de recursos_web.h
 *   GET  /api/estado               estado atual em JSON (estado_json.h);
 *                                  a pagina chama ao abrir, entao fechar e
 *                                  reabrir o navegador nao perde nada
 *   POST /api/carregar             CORPO = bytes crus do arquivo de config,
 *                                  como vieram do seletor de arquivo (pode
 *                                  ter BOM e CRLF). Ler Content-Length
 *                                  bytes, terminar com '\0' e passar para
 *                                  config_carregar_texto (config.h).
 *   POST /api/avancar, /api/retroceder, /api/executar_tudo, /api/editar
 *   GET  /api/gantt                SVG da JANELA atual (gantt.h,
 *                                  GANTT_JANELA_TELA ticks)
 *   GET  /api/exportar_svg         SVG da simulacao INTEIRA, com cabecalho
 *                                  Content-Disposition: attachment para o
 *                                  navegador baixar (req. 2.4)
 *
 * 'sim' precisa continuar valido durante toda a vida do servidor.
 *
 * Abre a porta, abre o navegador e fica atendendo pedidos ate o programa ser
 * fechado. So retorna se NAO conseguiu abrir a porta (ex. ja em uso), e
 * nesse caso devolve 0. */
int servidor_iniciar(Simulacao *sim, int porta);

/* Abre o navegador padrao no endereco do simulador. E chamada pelo proprio
 * servidor_iniciar, depois que a porta ja esta escutando. */
void servidor_abrir_navegador(int porta);

#endif
