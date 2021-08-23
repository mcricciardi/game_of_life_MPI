# [Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)
***
# Programmazione Concorrente, Parallela e su Cloud
## Università degli studi di Salerno
#
| Docenti      | Studente |
| ----------- | ----------- |
| Prof. Vittorio Scarano      | Maria Cristina Ricciardi        |
| Carmine Spagnuolo Ph.D.   | Matr. 0522500406        |

### **Anno Accademico 2020/2021**
***

## **Sommario** 
 1. [**Introduzione**](#introduzione)
 2. [**Descrizione del progetto**](#descrizione-del-progetto)
 3. [**Implementazione**](#implementazione)
 4. [**Note sulla compilazione ed esecuzione**](#note-compilazione-esecuzione)
 5. [**Note sull'implementazione**](#note-implementazione)
 6. [**Conclusioni**](#conclusioni)
 
***

## **Introduzione**
John Conway, matematico inglese, sviluppò questo gioco con l'intenzione di rappresentare un automa cellulare, basato su piccole **regole** di vita e morte. 

## **2. Descrizione del progetto**
Il progetto ha lo scopo di implementare **Game of Life** attraverso l'utilizzo di una matrice non quadrata, dove ogni **cella**, elemento della matrice, evolve secondo precise regole. La matrice verrà suddivisa in sottomatrici e inviate dal **MASTER** a diversi processi chiamati **SLAVES**. Gli SLAVES, compreso il MASTER, calcoleranno quanti vicini vivi ogni cella ha, per determinare se cambiare il suo stato da **LIVE** a **DEATH** o viceversa. Dopodiché i risultati saranno inviati al MASTER che provvederà a stampare la nuova matrice. Questo procedimento viene eseguito per diverse iterazioni.

## **3. Implementazione**
Il programma è stato sviluppato in **C** e la comunicazione parallela tra i processi avviene tramite la libreria **MPI (Message Passing Interface)**.

### Inizializzazione MPI  
```c
    /* inizializzazione MPI */
    MPI_Init(&argc, &argv);
    /* assegna un rank ad ogni processo */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* assegna un numero totale dei processi */
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);;
```

### Chiusura programma
```c
    //chiudi il programma se il numero di task non è maggiore o uguale a 2
    if(!(num_tasks >= 2)){
        printf("Il numero di task deve essere maggiore o uguale a 2.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
```
### Creazione matrice 
E' compito del MASTER allocare memoria e inizializzare la matrice
```c
    if(rank == MASTER){
        /* alloco memoria per la matrice principale creata dal master */
        matrix = (char*) malloc(rows * cols * sizeof (char*));
        /* in maniera random, la matrice viene popolata di caratteri 'D' (death) e 'L' (live) */
        randomMatrix(matrix, rows, cols);
        /* stampo a video la matrice creata */
        printMatrix(matrix, rows, cols, "MATRICE ORIGINALE");
        /* stampo i dati necessari per dividere equamente la matrice tra i tutti i processi */
        printVars(data, num_tasks);
    }
```
## **Note sulla compilazione ed esecuzione**
#### Compilazione
```
mpicc -o mpi game_of_life.c
```
#### Esecuzione
```
mpirun --allow-run-as-root -np <number of processes> mpi
```
dove <number of processes> è il numero di processori che deve essere un numero intero maggiore o uguale a due.
## **Note sull'implementazione**

| Array Smoothing | Nome e Cognome | Data di consegna |
| --- | --- | --- | --- |
# Descrizione della soluzione
10 righe
# Codice
<!--```c
#include <mpi.h>
#include <stdio.h>
int main(int argc, char** argv) {
MPI_Init(NULL, NULL);
int world_size;
MPI_Comm_size(MPI_COMM_WORLD, &world_size);
int world_rank;
MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
char processor_name[MPI_MAX_PROCESSOR_NAME];
int name_len;
MPI_Get_processor_name(processor_name, &name_len);
printf("Hello world from processor %s, rank %d out of %d processors\n",
processor_name, world_rank, world_size);
MPI_Finalize();
}
```-->
## Note sulla compilazione
## Note sull'implementazione
# Risulati
I={some value}
P={some value}
### K = {some value}
#### Scalabilità debole N={some value}
#### Scalabilità forte N={some value}
### K/2 = {some value}
#### Scalabilità debole N={some value}
#### Scalabilità forte N={some value}
### 2K = {some value}
#### Scalabilità debole N={some value}
#### Scalabilità forte N={some value}
## Descrizione dei risultati
# Conclusioni
