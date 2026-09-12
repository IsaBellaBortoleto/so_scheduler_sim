#ifndef TCB_H
#define TCB_H


#define MAX_ATIVACOES 10

#define MAX_EVENTOS_TXT 256

typedef enum {
    EST_INATIVA = 0,
    EST_PRONTA,
    EST_EXECUTANDO,
    EST_SUSPENSA,
    EST_CONCLUIDA
} EstadoTarefa;

const char *estado_nome(EstadoTarefa e);


typedef struct {
    
    int  id;                         
    unsigned char r, g, b;         
    int  ingresso;                    
    int  duracao;                    
    int  periodo;                    
    int  prazo;                     
    char eventos[MAX_EVENTOS_TXT];   


    EstadoTarefa estado;
    int  exec_restante;    
    int  quantum_restante;  
    int  ativacao;          
    int  deadline_abs;      
    int  ativacoes;        
    int  perdeu_prazo;     
    int  cpu;              
    int  sorteada;          
} TCB;


typedef struct {
    int id;
    int tarefa;           /* índice no vetor de tarefas, ou -1 = DESLIGADA   */
    int ticks_desligada;  /* acumulador para o relatório final               */
} CPU;


typedef struct {
    char algoritmo[16];  
    int  quantum;         
    int  ncpus;
    int  ntarefas;
    int  tick;            
    TCB *tarefas;       
    CPU *cpus;      
} Estado;

Estado estado_clonar(const Estado *e);
void   estado_liberar(Estado *e);

#endif 
