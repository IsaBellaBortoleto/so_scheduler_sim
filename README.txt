Esqueleto do Simulador de SO Multitarefa (Projeto A) em C

COMO COMPILAR
  make            -> gera ./simulador (build de desenvolvimento, com warnings)
  make release    -> gera ./simulador estatico, sem depender de libs na
                      maquina de destino (requisito 5 do enunciado)
  make clean      -> limpa os .o e o binario

COMO RODAR (depois de implementar os TODOs)
  ./simulador tests/exemplo_simples.txt
  ./simulador tests/exemplo_simples.txt --completo

ESTRUTURA
  include/tcb.h           struct TCB (Task Control Block) e enum de estado da tarefa
  include/clock.h         relogio global (ticks)
  include/cpu.h           processadores + fila de prontos global
  include/scheduler.h     interface plugavel de escalonador (tabela de algoritmos)
  include/config_parser.h parser do arquivo de configuracao (texto plano)
  include/history.h       snapshots para avancar/retroceder + edicao manual
  include/view.h          interface abstrata de visualizacao
  include/gantt_export.h  geracao do SVG final da simulacao completa
  include/error.h         mensagens de erro centralizadas

  src/clock.c             implementacao do relogio global
  src/cpu.c               implementacao dos processadores + fila de prontos
  src/scheduler.c         tabela de escalonadores registrados
  src/scheduler_rm.c      Rate Monotonic preemptivo
  src/scheduler_edf.c     Earliest Deadline First preemptivo
  src/config_parser.c     parser do arquivo de configuracao
  src/history.c           snapshots para avancar/retroceder
  src/view_console.c      implementacao padrao: gantt ao vivo no terminal via ANSI (zero deps)
  src/gantt_export.c      gera o SVG final (texto puro, sem lib de imagem)
  src/error.c             mensagens de erro centralizadas
  src/main.c              liga tudo: parse de argumentos + loop principal

  O Makefile ja compila com "-Iinclude", entao os .c continuam incluindo
  os headers so pelo nome (ex. #include "tcb.h"), sem precisar de
  caminho relativo.

NOTA SOBRE A VISUALIZACAO GRAFICA
  view.h e uma interface abstrata de proposito: a implementacao padrao
  (view_console.c) desenha o gantt no terminal com codigos ANSI, o que
  satisfaz o requisito 5 (nada extra pra instalar) da forma mais direta
  possivel. Se depois quiser uma janela grafica "de verdade" (nao so o
  terminal), da pra criar um view_win32.c (Windows, usando GDI, que ja
  vem com o SO) ou um view_x11.c (Linux) implementando as mesmas
  funcoes, e trocar so isso no Makefile -- o nucleo da simulacao nao
  muda nada.

Todos os arquivos .c/.h tem comentarios TODO explicando o que cada
funcao precisa fazer, seguindo os requisitos do enunciado (numeros
entre parenteses nos comentarios = numero do requisito no PDF).
