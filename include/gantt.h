#ifndef GANTT_H
#define GANTT_H

#include <stdio.h>
#include "kernel.h"

/* ----------------------------------------------------------------------------
 * DECISAO DE PROJETO (opcao A): o grafico de Gantt e desenhado SOMENTE aqui,
 * em C. A pagina web nao desenha nada -- ela exibe o SVG que o servidor
 * devolve. A MESMA funcao produz:
 *
 *   - a tela ao vivo (req. 2.3): so uma janela dos ultimos ticks;
 *   - o arquivo exportado (req. 2.4): a simulacao inteira, sem limite.
 *
 * Com uma implementacao so, as regras de desenho (cores, pronta sem cor,
 * suspensa preta, eixo Y, marcadores, CPU desligada) existem em um lugar
 * so, e a imagem exportada e identica a da tela por construcao.
 * --------------------------------------------------------------------------*/

/* Quantos ticks a tela ao vivo mostra (os ultimos, ate o tick atual).
 *
 * Por que nao mostrar tudo sempre: com um arquivo do porte do teste do
 * professor (~4 KB, ~190 tarefas), o SVG completo chega a 2,8-7,5 MB e
 * 30-78 mil elementos. Reenviar e redesenhar isso a cada clique em
 * "avancar" trava a pagina. Com 60 ticks, cada passo fica em ~150 KB.
 * Para ver ticks antigos: retroceder, ou exportar o SVG completo. */
#define GANTT_JANELA_TELA 60

/* Escreve em 'saida' o SVG dos ticks [tick_ini, tick_fim] do historico.
 *   - tela ao vivo:  tick_ini = max(0, tick_atual - GANTT_JANELA_TELA + 1),
 *                    tick_fim = tick_atual
 *   - exportacao:    tick_ini = 0, tick_fim = n_hist - 1
 *
 * Recebe FILE* (e nao um caminho) porque quem chama decide o destino: o
 * servidor precisa dos bytes para mandar pelo socket, o autoteste grava num
 * arquivo. A funcao NAO abre nem fecha 'saida'.
 *
 * ATENCAO, para o servidor: NAO use tmpfile() para obter esse FILE*. No
 * Windows, o msvcrt.dll cria o temporario na raiz do disco (C:\) e falha com
 * "Permission denied" para usuarios comuns -- testado com o gcc do projeto.
 * Use um arquivo com nome dentro de getenv("TEMP") e apague depois de enviar.
 *
 * Retorna 1 em sucesso, 0 se o intervalo for invalido ou o historico estiver
 * vazio. */
int gantt_svg(const Simulacao *s, FILE *saida, int tick_ini, int tick_fim);

#endif
