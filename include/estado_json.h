#ifndef ESTADO_JSON_H
#define ESTADO_JSON_H

#include <stddef.h>
#include "kernel.h"
//SUPONDO INTERFACE EM HTTP/JSON
/* Serializa o estado ATUAL da simulacao (s->atual, ou seja, o tick
 * corrente) para JSON, pra pagina web desenhar o gantt e os paineis.
 * Escreve em 'buf' (ate 'tam' bytes, sempre terminado em '\0').
 * Retorna o numero de bytes escritos (sem contar o '\0'), ou -1 se
 * 'buf' for pequeno demais (nesse caso, tenta de novo com um buffer
 * maior -- nao escreve parcialmente). */
int estado_json_atual(const Simulacao *s, char *buf, size_t tam);

/* Mesma ideia, mas para um snapshot especifico do historico
 * (indice 0..s->n_hist-1) -- util se a pagina quiser "saltar" direto
 * pra um tick em vez de só avançar/retroceder um de cada vez. */
int estado_json_historico(const Simulacao *s, int indice, char *buf, size_t tam);

/* Serializa erros/avisos de um Diagnostico (usado na resposta de
 * /api/carregar quando o arquivo de configuracao tem problema --
 * requisitos 3.3.6/3.3.7: motivo claro pro usuario). */
int diagnostico_json(const Diagnostico *d, char *buf, size_t tam);

#endif