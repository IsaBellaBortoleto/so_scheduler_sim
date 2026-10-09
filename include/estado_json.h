#ifndef ESTADO_JSON_H
#define ESTADO_JSON_H

#include <stddef.h>
#include "kernel.h"

/* Serializa o estado do tick ATUAL (s->atual) para JSON: cabecalho, tabela
 * de CPUs, tabela de tarefas e ultimo evento. O formato esta documentado no
 * topo de web/app.js.
 *
 * O Gantt NAO vai no JSON: ele vem pronto como SVG (ver gantt.h). Por isso a
 * pagina nao precisa mais do historico de ticks, e a antiga
 * estado_json_historico() foi removida.
 *
 * Tamanho: com o arquivo de teste do professor (~4 KB, ~190 tarefas), o JSON
 * fica na casa de 60 KB. Quem chama deve comecar com um buffer de pelo menos
 * 64 KB e dobrar enquanto a funcao devolver -1.
 *
 * Retorna o numero de bytes escritos (sem o '\0'), ou -1 se 'buf' for
 * pequeno demais -- nesse caso nao escreve nada pela metade, porque um JSON
 * cortado quebra a pagina sem mensagem de erro nenhuma. */
int estado_json_atual(const Simulacao *s, char *buf, size_t tam);

/* Erros e avisos do carregamento, para a resposta de /api/carregar
 * (req. 3.3.6/3.3.7: o motivo precisa chegar claro ao usuario).
 *
 * Cuidado ao montar o JSON a mao: as mensagens podem conter aspas e barras
 * invertidas (ex. um valor invalido copiado do arquivo). Escapar " e \ e
 * quebras de linha, senao o JSON fica invalido. Mesma regra vale para
 * "ultimo_evento" e "eventos" em estado_json_atual. */
int diagnostico_json(const Diagnostico *d, char *buf, size_t tam);

/* estado_json_atual mais o campo "avisos":[...] no fim, tirado de d. Usada
 * na resposta de /api/carregar: carregar com sucesso tambem pode gerar
 * avisos (ex. tarefa aperiodica ignorada, req. 4.4) e eles precisam chegar
 * ao usuario. Com d == NULL o campo nao aparece. */
int estado_json_com_avisos(const Simulacao *sim, const Diagnostico *d,
                           char *buf, size_t tam);

#endif
