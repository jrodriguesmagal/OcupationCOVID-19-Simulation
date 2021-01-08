/*
trabalho final - programação concorrente
Emergencia médica covid - 19

JEAN RODRIGUES MAGALHAES
15/0079923
 */
//bibliotecas utilizadas
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include<stdbool.h>
#include "string.h"

//definições
#define N_CAPACIDADE_EMERGENCIA 100 //numero de pessoas que a emergencia consegue atender
#define N_PACIENTES 110              //numero de pacientes que procuraram a emergencia
#define N_LEITOS_NORMAIS 15         // numeros de leitos normais para aqueles pacientes pouco graver que precisarem de internação
#define N_LEITOS_UTI 5              //numero de leitos de uti para pacientes muito graves


//declaração de variavei globlais que irão gerar as condições de corrida
int contador_leitos_normais = 0, contador_leitos_uti = 0;

bool flag_cons_1 = false, flag_cons_2 = false, flag_cons_3 = false; //flags que indicam se o consultorios estao ocupados ou nao
bool flag_cons_especial = false; //flag que libera ou bloqueia o consultorio especial caso um paciente urgente chegue


//semforos
sem_t sem_emergencia; //pessoas na emergencia
sem_t sem_sala_triagem; // sala de triagem (temos uma sala para triar os pacientes a direcionar aos 3 consultorios)
sem_t sem_paciente_triagem; // paciente da triagem
sem_t sem_fazendo_triagem; // indica o procedimento de triagem



//funções das threads
void* triagem(void*);
void* func_pacientes(void*);

//funções que auxiliam as funções das threads
bool gera_gravidade_caso(bool gravidade); //funcao que diz a gravidade do caso na triagem
void consultorios_medicos(int id_paciente, bool gravidade); //função que gerencia os consultorios consultorios medicos
void reposta_triagem(int id_paciente, bool gravidade); //funcao que imprime os resultados da triagem dos pacientes
void sala_espera_medico(int id_paciente, bool gravidade); //funcao que "gera a fila" de pessoas que estao aguardando serem chamados por um dos médicos
bool verifica_leito(int id_paciente, char str[]);  //func que verifica se tem leitos disponiveis caso o paciente precise de internação
void resultado_consulta(int id_paciente, bool gravidade); //func que fala de o paciente precisa de internação e chama a func de verifica_leito


//locks
pthread_mutex_t lock_gravidade = PTHREAD_MUTEX_INITIALIZER;  //lock para travar o acesso da sala urgencia na verificação se ela está ocupada ou nao
pthread_mutex_t lock_consultorios = PTHREAD_MUTEX_INITIALIZER; //lock dos consultorios
pthread_mutex_t lock_leito_normal = PTHREAD_MUTEX_INITIALIZER; //lock do contador de leitos normais

pthread_mutex_t lock_leito_uti = PTHREAD_MUTEX_INITIALIZER; //lock do contador de leitos de UTI
// Função main
int main() {


 //variaveis utilizadas na gerção das threads
 int i, id[N_PACIENTES];
 pthread_t thread_pacientes[N_PACIENTES], thread_triagem;



 // inciando semaforos
 sem_init(&sem_emergencia, 0, N_CAPACIDADE_EMERGENCIA); // semaforo da emergenia com N_CAPACIDADE_EMERGENCIA
 sem_init(&sem_sala_triagem, 0, 1); //sasla de triagem com atendimento de um paciente por vez
 sem_init(&sem_paciente_triagem, 0, 0); //semaforo para avisar que tem paciente para ser triado
 sem_init(&sem_fazendo_triagem, 0, 0); //semaforo que representa o paciente fazendo a triagem


 // geração das threads dos pacientes
 for (i = 0; i < N_PACIENTES; i++) {
   id[i] = i;
   pthread_create(&thread_pacientes[i], NULL, func_pacientes, (void*) &id[i]);
 }

 //triagem
 pthread_create(&thread_triagem, NULL, triagem, NULL);


 for (i = 0; i < N_PACIENTES; i++)
 pthread_join(thread_pacientes[i], NULL);

 return 0;
}


//func da triagem que usa semaforo para chamar o enfermeiro e triar o paciente que estiver esperando
void* triagem(void* args) {
 // funcia para sempre para assegurar que sempre que chegar um paciente ele seja triado (desde que esteja na capacidade da emergencia)
 while(1) {
   sem_wait(&sem_paciente_triagem); // espera por pacientes para serem triados
   sleep(2); //tempo de triagem padrão
   sem_post(&sem_fazendo_triagem); // libera triagem (triagem concluida)
 }

}
void* func_pacientes(void* id_thread) {
 //identificção dos pacientes
 int id_paciente = *(int*) id_thread;

 //variavel de cada thread que vai receber o dado se o paciente é grave ou nao
 bool gravidade = false;


 //faz com que pacientes sejam mandados para outro hospital caso a emergencia esteja super lotada
 if(sem_trywait(&sem_emergencia) != 0) {
   printf("Emergencia Cheia, paciente %d foi direcionado para outro hospital.\n", id_paciente);
   pthread_exit(0);
 }


 //procedimento para fazer com que os pacientes passem pela triagem em ordem de chegada

   printf("O paciente %d deu entrada no Hospital.\n", id_paciente);
   sem_wait(&sem_sala_triagem); //entra na sala de espera e bloqueia o semaforo para so uma pessoa por vez entrar
   sem_post(&sem_emergencia); // Libera espaço na emergencia
   sem_post(&sem_paciente_triagem); // Avisa pro enfermeiro que tem gente esperando triagem
   sem_wait(&sem_fazendo_triagem); // ocorrendo triagem


   //depois que a triagem a é feita, o paciente sabe seu status de gravidade e para isso chamamos a funcao "geradora de status gravidade"
 gravidade = gera_gravidade_caso(gravidade);

 sem_post(&sem_sala_triagem); //libera a sala de triagem

 reposta_triagem(id_paciente, gravidade); // imprime a gravidade do paciente

 sala_espera_medico(id_paciente, gravidade); //paciente vai para a sala de espera dos consultorios

 pthread_exit(0);
}


//nesta func é gerada a fila para casos nao muito urgentes e tem a condição de prioridade para casos graves
void sala_espera_medico(int id_paciente, bool gravidade){
  // antes de entrar na fila de espera do consultorio, verificamos a gravidade desse paciente que acabou de chegar da triagem, para mandar ou não pro consultorio especial
  if( gravidade == true){
      pthread_mutex_unlock(&lock_gravidade); //asseguramos que nenhum paciente grave concorra pela sala especial e entrem juntos no consultorio
                                              // se o consultorio especial ja estiver cheio, o novo paciente de muita urgencia vai ser o primeiro a ter acesso aos outros quando forem liberados

      consultorios_medicos(id_paciente, gravidade);// acesso prioritario aos consultorios pelos pacientes urgentes
  }

  //fila de pacientes pouco urgentes
  pthread_mutex_lock(&lock_consultorios);
    consultorios_medicos(id_paciente, gravidade); //um de cada vez verifica qual consultorio "o chamou" depois de liberar vaga

}

//aqui tratamos quem vai entrar em cada consultorio, caso ele entre aqui, a thread vai ficar em loop aguardando um atendimento acabar
void consultorios_medicos(int id_paciente, bool gravidade){
  while (1) {

    //só entra nesse consultorio se for muito urgente
    if( gravidade == true && flag_cons_especial == false ){
      printf("O paciente %d está no consultório de MUITA URGÊNCIA.\n", id_paciente);

      flag_cons_especial= true; //assinala consultorio em uso
      pthread_mutex_unlock(&lock_gravidade); //libera a condição de corrida pelo consultorio especial entre paciente muito urgentes
      pthread_mutex_unlock(&lock_consultorios); //libera outros pacientes que estao na fila normal
      sleep(4); // tempo de consulta padrao para seguir os procedimento de um paciente grave
      verifica_leito(id_paciente, "uti"); //chama a funçãp que verifica se ainda há leitos de UTI neste hospital

    }

      //só entra se estiver vazio e usa o lock bloquear consultorios para acegurar que ninguem entre junto a ele
      if (flag_cons_1 == false) {
        printf("O paciente %d está no consultório 1.\n", id_paciente);
        flag_cons_1 = true; //assinala consultorio em uso
        pthread_mutex_unlock(&lock_consultorios);
        sleep(rand() % 10); //gera numeros aleatorio para tempo de consulta nesse consultorios
        flag_cons_1 = false;
        resultado_consulta(id_paciente, gravidade); //libera o resultado da consulta deste paciente
      }

      //só entra se estiver vazio e usa o lock bloquear consultorios para acegurar que ninguem entre junto a ele
      if (flag_cons_2 == false) {
        printf("O paciente %d está no consultório 2.\n", id_paciente);
        flag_cons_2 = true; //assinala consultorio em uso
        pthread_mutex_unlock(&lock_consultorios);
        sleep(rand() % 10); //gera numeros aleatorio para tempo de consulta nesse consultorios
        flag_cons_2 = false;
        resultado_consulta(id_paciente, gravidade); //libera o resultado da consulta deste paciente
      }

      //só entra se estiver vazio e usa o lock bloquear consultorios para acegurar que ninguem entre junto a ele
      if (flag_cons_3 == false) {
        printf("O paciente %d está no consultório 3.\n", id_paciente);
        flag_cons_3 = true; //assinala consultorio em uso
        pthread_mutex_unlock(&lock_consultorios);
        sleep(rand() % 10); //gera numeros aleatorio para tempo de consulta nesse consultorios
        flag_cons_3 = false;
        resultado_consulta(id_paciente, gravidade); //libera o resultado da consulta deste paciente
      }

  }

}


// aqui usamos um pouco da probabilidade para gerar resultados diferentes entre consultas de pacientes "pouco urgentes"
void resultado_consulta(int id_paciente, bool gravidade){

  //de acordo com o Ministério da saúde em torno de 10% dos pacientes precisam ser hospitalizados
  int probabilidade_internacao = rand() % 100;

  if( gravidade == true){ //caso execepcional de que pacientes muito urgentes foram atendidos em consultorio nao especial
      verifica_leito(id_paciente, "uti"); //chama a funçãp que verifica se ainda há leitos de UTI neste hospital
  }

  // se estiver dentro desses 10% aleatorios, séra verificado a possibilidade de internação
  if (probabilidade_internacao >= 90) {
    printf("O paciente %d precisa ficar internado\n", id_paciente);
    verifica_leito( id_paciente, "normal"); //verifica se ainda há leitos neste hospital para pacientes com menos gravidade
  }else{
    //caso seja um paciente que possa continuar o tratamento em casa, ele é mandado pra casa e sua thread é encerrada
    printf("O paciente %d fará o tratamento em de recupeção da COVID-19 em casa\n", id_paciente);

  }
  pthread_exit(0);
}


//como todo hospital, há lotações maximas que cada hospital pode atender tanto para uti quanto para leito normal
//de acordo com a gravidade do , voce tem que direcionar mais esforços para um do que para outros, por isso, geralmente, temos mais leitos normais num hospital do que de uti
bool verifica_leito(int id_paciente, char str[]){

  //se for de uti, vai competir leito com pacientes muito urgentes. Se não tiver mais vaga, vai ser transferido para outro hospital
  if (str == "uti") {
    if (contador_leitos_uti < N_LEITOS_UTI) {
        contador_leitos_uti++;
      printf("O paciente %d foi internado na UTI ----- total de vagas ocupadas na UTI deste hospital é: %d\n", id_paciente, contador_leitos_uti);
    }else{
      pthread_mutex_unlock(&lock_leito_uti);
      printf("O paciente %d deverá ser transferido para outro hospital imediatamente ----- total de vagas ocupadas na UTI deste hospital é: %d\n", id_paciente, contador_leitos_uti);
    }
    flag_cons_especial = false;
    pthread_exit(0);
  }

  //se for de leito normal, vai competir leito com pacientes que aguardam este tipo de leito. Se não tiver mais vaga, vai ser transferido para outro hospital
  if (str == "normal") {
    pthread_mutex_lock(&lock_leito_normal);
    if (contador_leitos_normais < N_LEITOS_NORMAIS) {
        contador_leitos_normais++;
    pthread_mutex_unlock(&lock_leito_normal);
      printf("O paciente %d foi internado ----- total de vagas ocupadas na internação deste hospital é: %d\n", id_paciente, contador_leitos_normais);
    }else{
      pthread_mutex_unlock(&lock_leito_normal);
      printf("O paciente %d deverá ser transferido para outro hospital ----- total de vagas ocupadas na internação deste hospital é: %d\n", id_paciente, contador_leitos_normais);
    }
    pthread_exit(0);
  }

}


//func que imprime resultado da triagem de acordo com a gravidade gerada
void reposta_triagem(int id_paciente, bool gravidade){

  printf("O paciente %d passou pela triagem e o caso é:", id_paciente);
  if (gravidade == false) {
    printf(" Pouco urgente\n");
  }else{
    printf(" MUITO URGENTE\n");
  }
  printf("O paciente %d foi em direção aos consultórios médicos.\n", id_paciente);


}


//usamos aqui um pouco da aleatoriedade para gerar casos MUITO GRAVES de COVID-19, esse numero geralmente é em torno de 3%, mas para conseguimos ve-los com mais frequecia aqui, utilizamos 10%
bool gera_gravidade_caso(bool gravidade){
  int aleatorio;

  aleatorio = rand() % 10;

  if (aleatorio >= 9) {
    gravidade = true;
  }else{
    gravidade = false;
  }

  return gravidade;
}
