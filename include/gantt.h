#ifndef GANTT_H
#define GANTT_H

#include "kernel.h"

/* A visualizacao "ao vivo" (requisitos 2.1-2.3) agora e a pagina web,
 * alimentada por estado_json.h + servidor.h -- entao esse arquivo fica
 * responsavel so pela saida que continua sendo tarefa do C puro:
 * o arquivo de imagem final exportado ao fim da simulacao (requisito 2.4).
 *
 * gantt_preparar_terminal()/gantt_terminal() saíram daqui: se quiser
 * manter uma visualizacao ANSI so como atalho de debug durante o
 * desenvolvimento (nada a ver com o que sera avaliado), da pra colocar
 * elas num modulo separado (ex. debug_terminal.h) pra nao confundir
 * com o que o enunciado pede. */

/* Gera o grafico de gantt final em SVG a partir do historico completo
 * da simulacao (requisito 2.4: representa a simulacao INTEIRA, sem
 * limite de tempo, e nao pode ser print de tela).
 * Retorna 1 em sucesso, 0 em erro (ex. nao conseguiu abrir 'caminho'). */
int gantt_svg(const Simulacao *s, const char *caminho);

#endif