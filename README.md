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
 4. [**Note sulla compilazione ed esecuzione**](#Notesullacompilazioneedesecuzione)
 5. [**Benchmark**](#Benchmark)
 6. [**Conclusioni**](#conclusioni)
 
***

## **Introduzione**
John Conway, matematico inglese, sviluppò questo gioco con l'intenzione di rappresentare un automa cellulare, basato su piccole **regole** di vita e morte. 

## **Descrizione del progetto**
Il progetto ha lo scopo di implementare **Game of Life** attraverso l'utilizzo di una matrice non quadrata, dove ogni **cella**, elemento della matrice, evolve secondo precise regole. La matrice verrà suddivisa in sottomatrici e inviate dal **MASTER** a diversi processi chiamati **SLAVES**. Gli SLAVES, compreso il MASTER, calcoleranno quanti vicini vivi ogni cella ha, per determinare se cambiare il suo stato da **LIVE** a **DEATH** o viceversa. Dopodiché i risultati saranno inviati al MASTER che provvederà a stampare la nuova matrice. Questo procedimento viene eseguito per diverse iterazioni.

## **Implementazione**
Il programma è stato sviluppato in **C** e la comunicazione parallela tra i processi avviene tramite la libreria **MPI (Message Passing Interface)**.
Il processo MASTER crea una matrice ed equamente invia le righe ai processi SLAVES. Tramite MPI_Scatterv, una routine di MPI, si inviano le singole righe e ogni processo, compreso il MASTER, non avrà lo stesso numero di righe, quindi il buffer da inviare avrà elementi diversi.
I processi sono raggruppati in una topologia in modo che ogni processo invierà ai suoi vicini a sinistra e destra, una copia delle righe superiore e inferiore della propria sottomatrice. Quindi il MASTER invierà la riga superiore all'ultimo processo e la riga inferiore al processo successivo.

**Esempio:**
MASTER riga top -> ultimo task
MASTER riga bottom -> task successivo

task n°1 SLAVE riga top -> al MASTER
task n°1 SLAVE riga bottom -> task n°2

ultimo task SLAVE riga top -> penultimo task
ultimo task SLAVE riga bottom -> al MASTER

Le righe fantasme sono utilizzate per calcolare quante celle vive ogni elemento della matrice possiede. Una volta calcolato le celle vive vicine, in base al conteggio, cambia lo stato della singola cella, D per DEATH o L per LIVE.
Fatto ciò, ogni processo invia tramite la routine MPI_Igatherv, la propria sottomatrice al MASTER.

### Variabili principali
```c
    /* rank di ogni processo
     * num_tasks il numero di processi complessivi
     * tag del messaggio
     */
    int rank, num_tasks, tag =0;
    
    /*
     * matrix è la matrice principale create e popolata dal MASTER
     * localMatrix è la sottomatrice locale utilizzata da tutti i processi
     * workMatrix è la matrice che contiene le righe fantasme e una copia della matrice locale,
     * serve per effettuare il cambio di stato di ogni elemento della matrice.
     */
    char *matrix, *localMatrix, *workMatrix;

    //numero di vicini vivi
    int *neighbours = (int*) malloc(sizeof (int*));
```
### Inizializzazione MPI  
```c
    /* inizializzazione MPI */
    MPI_Init(&argc, &argv);
    /* assegna un rank ad ogni processo */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* assegna un numero totale dei processi */
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
    //rappresenta lo stato di un'operazione di ricezione
    MPI_Status status;
    //rappresenta un handle su un'operazione non bloccante
    MPI_Request request;
```
### Creazione matrice 
E' compito del MASTER allocare memoria e inizializzare la matrice principale
```c
    if(rank == MASTER){

        /* divido la matrice secondo il numero dei task e inizializzo la struttura dati */
        splitMatrix(rows * cols, num_tasks, data);

        /* alloco memoria per la matrice principale creata dal master */
        matrix = (char*) malloc(rows * cols * sizeof (char*));

        /* in maniera random la matrice viene popolata di caratteri 'D' (death) e 'L' (live) */
        randomMatrix(matrix, rows, cols);

        /* stampo a video la matrice creata */
        printMatrix(matrix, rows, cols, "MATRICE ORIGINALE");

        /* stampo i dati necessari per dividere equamente la matrice tra i tutti i processi */
        printVars(data, num_tasks);
    }
```
### Invio le sottomatrici agli SLAVES e MASTER
Utilizzo una struttura dati per dividere equamente la matrice tra i processi.
Per ogni processo indico quante righe ed elementi deve avere la sottomatrice.
```c
//struttura dati utilizzata per dividire equamente la matrice tra i processi
typedef struct{
    /* conta quante righe saranno inviate a ogni processo*/
    int *rowcounts;
    /* rappresenta gli scostamenti, è un numero intero che indica dove inizia ogni blocco nella matrice che viene vista come un array contiguo. */
    int *displacements;
    /* conta il numero di elementi di ogni riga */
    int *counts;
    /* il resto dell'operazione modulare di (ROWS ) % tasks, dove ROWS sono le righe totali della matrice e tasks il numero complessivi dei processi */
    int rem;
} Data_scatter;
```
Attraverso la funzione splitMatrix, ottengo i dati della struttura necessari per inviare le righe ad ogni processo.
```c
   /* divido la matrice secondo il numero dei task e inizializzo la struttura dati */
void splitMatrix(int dim_matrix, int tasks, Data_scatter *data){
    
    data->rem = (ROWS ) % tasks;
    int sum = 0;
    for (int i=0; i<tasks; i++){
        data->rowcounts[i] = (ROWS) / tasks;
        if(data->rem > 0){
            data->rowcounts[i]++;
            data->rem--;
        }
        data->displacements[i] = sum*COLS;
        sum += data->rowcounts[i];
        data->counts[i] = data->rowcounts[i]*COLS;
    }

}
```
### Raggruppare i processi in una topologia
Ho la necessità di inviare le celle fantasma ai processi in modo che ogni cella possa contare i propri otto vicini sia in orizzontale, verticale e in diagonale. Quindi per fare ciò ogni processo invia una riga superiore al suo vicino processo a sinistra e una riga inferiore a quello a destra. Tramite la topologia e in particolare la routine MPI_Cart_shift, posso calcolare i processi vicini.
```c
    /*
     * dati per la topologia
     * */
    //il ranking può essere riordinata (vero) o meno (falso)
    int reorder = 0;
    //numero di dimensioni della griglia cartesiana (intero)
    int ndims = 1;
    //array logico di dimensioni ndim che specifica se la griglia è periodica (vera) o meno (falsa) in ogni dimensione
    int period[] = {1};
    //nuovo comunicatore
    MPI_Comm cart_comm;
    //variabili che indicano i processi precedenti e successori di un dato processo
    int prev, next;
    //numero di processi per ogni dumensione, qui numero di tasks per la prima dimensione
    int dims[1] = {num_tasks};
    MPI_Dims_create(num_tasks, 1, dims);

    //crea la nuova topologia
    MPI_Cart_create(MPI_COMM_WORLD, ndims, dims, period, reorder, &cart_comm);

    //sposta virtualmente la topologia cartesiana di un comunicatore (creato con MPI_Cart_create) nella dimensione specificata.
    MPI_Cart_shift(cart_comm, 1, 1, &prev, &next);
```
### Esecuzione ad ogni iterazione
```c
    //iterazioni
    for (int iter = 1; iter <= NITER; iter++) {

        /* tramite scatterv invio le sottomatrici ai task
        * ogni sottomatrice ha una dimensione diversa
        * */
        MPI_Scatterv(matrix, data->counts, data->displacements, MPI_CHAR,
                     localMatrix, data->counts[rank], MPI_CHAR,
                     MASTER, MPI_COMM_WORLD);

        /* SEND
         * Invio lA PRIMA e ULTIMA riga ai task
         */
        MPI_Send(&localMatrix[last_row], COLS, MPI_CHAR, next, tag, cart_comm);
        MPI_Send(&localMatrix[first_row], COLS, MPI_CHAR, prev, tag, cart_comm);

        /*
         * Dopo aver inviato le righe fantasme ai task, aspettano tutti alla barriera
         */
        printf("\n");
        MPI_Barrier(cart_comm);

        /* RECEIVE
         * ogni processo riceve le righe fantasme dal processo precedente e quello successivo
         */
        MPI_Recv(arr_first_row, COLS, MPI_CHAR, prev, tag, cart_comm, &status);
        MPI_Recv(arr_last_row, COLS, MPI_CHAR, next, tag, cart_comm, &status);

        /*
         * aggiunge alla workmatrix le righe fantasma e la sottomatrice locale ricevuta dal MASTER
         */
        mergeRowsAtMatrix(arr_first_row, arr_last_row, localMatrix, workMatrix, data->rowcounts[rank]+2, COLS);

        //applico le regole
        rules(workMatrix, localMatrix,  data->rowcounts[rank]+2,  COLS, neighbours);

        /*
         * Viene utilizzato una gatherv non bloccante.
         * Ogni processo invia la propria matrice locale al MASTER
         */
        MPI_Igatherv(localMatrix,data->counts[rank], MPI_CHAR,
                     matrix, data->counts, data->displacements, MPI_CHAR,
                     MASTER, cart_comm, &request);
        /*
         * attende il completamento di gatherv
         */
        MPI_Wait(&request, MPI_STATUS_IGNORE);

        /*
         * Il MASTER stampa la matrice modificata con il numero di iterazione
         */
        if(rank == MASTER) {
            sprintf(msg, "MATRICE ORIGINALE MODIFICATA iterazione n° %d",
                    iter);
            printMatrix(matrix, ROWS, COLS, msg);
        }
    }
```
### Regole
Queste sono le regole che Game of Life applica alle celle vive o morte:
```c
/*
 * REGOLE:
 * 1. cella morta 'D' con esattamente 3 vicini vivi, diventa viva 'L'
 * 2. cella viva 'L' con 2 o 3 vicini vivi, continua a vivere
 * 3. cella viva 'L' con più di 3 vicini vivi 'L' OPPURE con meno di 2 vicini vivi 'L', muore 'D'
 *
 * Applica le regole del game of life, le modifiche ad ogni elemento sono applicate alla matrice locale.
 * Non tiene conto degli elementi della prima e ultima riga della workmatrix che sono fantasma
 */
void rules(char *matrix, char *oldMatrix, int rows, int cols, int *neighbours){
    char state;
    for (int x = 0, x1 = 1; x < rows-2; x++, x1++)
        for (int y = 0; y < COLS; y++) {
            state = oldMatrix[x*COLS+y];
            countNeighbours(matrix, x1, y, rows, cols, neighbours);

            if(state=='D' && *neighbours==3)
                oldMatrix[x*COLS+y] = 'L';
            else if (state == 'L' && (*neighbours<2 || *neighbours>3))
                oldMatrix[x*COLS+y] = 'D';
            else
                oldMatrix[x*COLS+y] = state;
        }
}
```
Conta i vicini vivi per ogni cella della matrice.
```c
/*
* viene inviata una matrice e delle coordinate (x,y) per calcolare quanti vicini vivi ci sono per ogni elemento.
* Non tiene conto degli elementi della prima e ultima riga che sono fantasma
* quindi x va da 1 a rows-2
*/
void countNeighbours(char *matrix, int x, int y, int rows, int cols, int *neighbours){
    *neighbours = 0;
 
    for (int i=-1; i<2; i++){
        for (int j=-1; j<2; j++){
            int col = (y + j + cols) % cols;
            int row = (x + i + rows) % rows;
        
            if(matrix[row*COLS+col] == 'L'){
                (*neighbours)++;
                //elimina sè stesso
                if(row == x && col == y)
                    (*neighbours)--;
            }
        }
    }
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

## Benchmark
Il programma è stato eseguito su un cluster di quattro istanze **m4.xlarge** Amazon EC2 con 4 vCPU e 16 GB di memoria su macchine Ubuntu Server 20.04 LTS (HVM), SSD Volume Type - ami-09e67e426f25ce0d7. Il cluster è quindi composto in totale da 16 vCPU e 64 GB di memoria.
Le metriche prese in considerazione per eseguire i test, sono i tempi di esecuzione in secondi, speedup, la weak scalability e la strong scalability.

**Speedup**

 Speedup = T1/Tn dove
* T1 è l'esecuzione del tempo di calcolo per un solo processore,
* Tn invece per più processori.

**Strong scalability**
 
Il numero di processori aumenta, mentre la dimensione del problema rimane costante, quindi il carico di lavoro per i processori viene ridotto.
 L'efficienza è stata calcolata mediante questa forumula:

 **Ep = T1/(P*Tp))*100%**

 
### Scalabilità forte per matrice 100x100
 
![](./imgs/Scalabilità%20forte%20100x100%20tempo.png)
 
 |     vCPU          |     1         |     2         |     4          |     6         |     8         |     10        |     12         |     14        |     16        |
|-------------------|---------------|---------------|----------------|---------------|---------------|---------------|----------------|---------------|---------------|
|     Tempo(s)      |     0,8239    |     0,5822    |     0,06166    |     1,1290    |     1,2261    |     1,6467    |      1,8485    |     1,5977    |     1,4767    |
|     Efficienza    |     100%      |     70,76%    |     33,40%     |     12,16%    |     8,40%     |     5%        |     3,71%      |     3,68%     |     3,49%     |
 
### Scalabilità forte per matrice 500x500
 
![](./imgs/Scalabilità%20forte%20500x500%20tempo.png)
 
 |     vCPU          |     1          |     2          |     4          |     6          |     8          |     10         |     12         |     14         |     16         |
|-------------------|----------------|----------------|----------------|----------------|----------------|----------------|----------------|----------------|----------------|
|     Tempo(ms)     |     1,83299    |     0,97676    |     0,85309    |     0,76484    |     0,61245    |     0,55952    |     0,55211    |     0,47232    |     0,50605    |
|     Efficienza    |     100%       |     93,83%     |     53,72%     |     39,94%     |     37,41%     |     32,76%     |     27,66%     |     27,72%     |     22,64%     |

### Scalabilità forte per matrice 1000x1000
 
![](./imgs/Scalabilità%20forte%201000x1000%20tempo.png)
 
 |     vCPU         |     1    |     2           |     4           |     6           |     8           |     10          |     12          |     14          |     16          |
|------------------|----------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|
|     Tempo(ms)    |          |     3,49495     |     3,16082     |     2,56097     |     2,16323     |     1,93874     |     1,86438     |     1,77438     |     1,67808     |

 
 
### Scalabilità debole 
 
 All'aumentare dei processi, si aumenta anche la dimensione del problema, il numero di righe in maniera uniforme.
 
 L'efficienza della scalabilità debole è data:
 **Ep = (T1/Tp)*100%** 
 
 
 |     vCPU               |     1           |     2           |     4           |     6           |     8           |     10          |     12          |     14          |     16          |
|------------------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|
|     Numero di righe    |     100         |     200         |     400         |     600         |     800         |     1000        |     1200        |     1400       |     1600        |
|     Tempo(ms)          |     0.014702    |     0.030274    |     0.324914    |     0.394626    |     0.515214    |     0.609765    |     0.737777    |     0.875828    |     0.820073    |
|     Efficienza         |     100%        |     48,56%      |     4,52%       |     3,72%       |     2,85%       |     2,41%       |     1,99%       |     1,68%       |     1,79%       |

# Conclusioni
 Nella scalabilità forte, vediamo che l'andamento del tempo di esecuzione non è costante, soprattutto nel primo grafico, vi sono dei picchi in cui il tempo aumenta per un certo numero di processori. Ciò non succede negli altri grafici, quando si aumenta la grandezza della matrice.
 Invece nella scalabilità debole l'efficienza del programma diminuisce man mano che aumenta la dimensione dell'input della matrice.
 A 2000 righe abbiamo un errore di memoria.
