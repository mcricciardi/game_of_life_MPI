#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
//numero del rank del processo master
#define MASTER 0
//numero di righe della matrice principale
#define ROWS 1000
//numero di colonne della matrice principale
#define COLS 500
//numero di iterazioni
#define NITER 10

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

int r(int min, int max);
void printMatrix(char *matrix, int rows, int cols, char* str);
void randomMatrix(char *matrix, int rows, int cols);
void countNeighbours(char *matrix, int x, int y, int rows, int cols, int *neighbours);
void printVars(Data_scatter *data, int num_tasks);
void splitMatrix(int dim_matrix, int num_tasks, Data_scatter *data);
void mergeRowsAtMatrix(char *top_arr, char *bottom_arr, char *old_matrix, char *new_matrix, int rows, int cols);//aggiungi le righe fantasme top e bottom alla matrice
void rules(char *matrix, char *oldMatrix, int rows, int cols, int *neighbours);

int main(int argc, char* argv[]) {
    /* rank di ogni processo
     * num_tasks il numero di processi complessivi
     * tag del messaggio
     */
    int rank, num_tasks, tag =0;

    //variebili che calcolano il tempo di esecuzione del programma
    double start, end;

    //variabili che indicano quante righe e colonne deve avere la matrice principale
    int rows = ROWS, cols = COLS;

    /*
     * matrix è la matrice principale create e popolata dal MASTER
     * localMatrix è la sottomatrice locale utilizzata da tutti i processi
     * workMatrix è la matrice che contiene le righe fantasme e una copia della matrice locale,
     * serve per effettuare il cambio di stato di ogni elemento della matrice.
     */
    char *matrix, *localMatrix, *workMatrix;

    //numero di vicini vivi
    int *neighbours = (int*) malloc(sizeof (int*));

    //array usato per la stampa di stringhe
    char msg[80];

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

    /*
     * Tutti i processi si fermano e ripartono quando tutti sono arrivati alla barriera
     */
    MPI_Barrier(MPI_COMM_WORLD);

    //************************** start time *********************************
    start = MPI_Wtime();

    /* alloco memoria per la struttura dei dati per la scatterv */
    Data_scatter *data = malloc(sizeof(Data_scatter));
    data->counts = (int*) malloc(num_tasks * sizeof (int*));
    data->rowcounts = (int*) malloc(num_tasks * sizeof (int*));
    data->displacements = (int*) malloc(num_tasks * sizeof(int*));
    /* divido la matrice secondo il numero dei task e inizializzo la struttura dati */
    splitMatrix(rows * cols, num_tasks, data);

    if(rank == MASTER){



        /* alloco memoria per la matrice principale creata dal master */
        matrix = (char*) malloc(rows * cols * sizeof (char*));

        /* in maniera random la matrice viene popolata di caratteri 'D' (death) e 'L' (live) */
        randomMatrix(matrix, rows, cols);

        /* stampo a video la matrice creata */
        printMatrix(matrix, rows, cols, "MATRICE ORIGINALE");

        /* stampo i dati necessari per dividere equamente la matrice tra i tutti i processi */
        printVars(data, num_tasks);
    }

    /* alloco memoria per la matrice locale che ogni task userà */
    localMatrix = (char *) malloc(data->rowcounts[rank] * COLS * sizeof(char *));

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

    /*
     * Tutti i processi si fermano e ripartono quando tutti sono arrivati alla barriera
     */
    MPI_Barrier(cart_comm);

    /*
     * indici per prelevare le righe fantasma
     */
    int last_row = (data->rowcounts[rank]-1)*COLS;
    int first_row = 0;

    //array utilizzati per ricevere le righe fantasma
    char arr_first_row[COLS], arr_last_row[COLS];

    //matrice con due righe in più, contiene la sottomatrice ricevuta con le righe fantasma
    workMatrix = (char *) malloc((data->rowcounts[rank]+2) * COLS * sizeof(char *));

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
                    iter-1);
            printMatrix(matrix, ROWS, COLS, msg);
        }
    }

    //***********************************************************end time
    /*
     * Tutti i processi si fermano e ripartono quando tutti sono arrivati alla barriera
     */
    MPI_Barrier(MPI_COMM_WORLD);

    //prende il tempo finale dell'esecuzione
    end = MPI_Wtime();

    //libero memoria
    free(localMatrix);
    free(workMatrix);
    free(data);

    //chiude la libreria MPI.
    MPI_Finalize();

    //Il MASTER calcola e stampa il tempo finale in ms
    if(rank == MASTER){

        printf("Time in ms %f\n", end-start);
    }
    return EXIT_SUCCESS;
}


/* divido la matrice secondo il numero dei task e inizializzo la struttura dati */
void splitMatrix(int dim_matrix, int tasks, Data_scatter *data){
    //  printf("NUMERO DI TASKS %d\n", tasks);
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
/*
 * restituisce un numero casuale tra minimo e massimo
 */
int r(int min, int max){
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}
/*
 * popola la matrice di elementi L e D
 */
void randomMatrix(char *matrix, int rows, int cols){
    enum life {LIVE, DEAD};

    for (int i=0; i<rows; i++){
        for (int j=0; j<cols; j++) {
            int ran = r(0, 1);
            if(ran == LIVE)
                matrix[i * cols + j] = 'L';
            else
                matrix[i * cols + j] = 'D';
        }
    }
}

//rows == num_tasks, cols == num elements dell'array
void printMatrix(char *matrix, int rows, int cols, char* str){
    printf("%s: %dx%d\n", str, rows, cols);
    for (int i=0; i<rows; i++){
        for (int j=0; j<cols; j++)
            printf("(%d)%c  ", i*cols+j,matrix[i*cols+j]);
        printf("\n");
    }
    printf("\n");
}
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
/*
 * stampo i dati usati per dividire la matrice principale in sottomatrici
 */
void printVars(Data_scatter *data, int num_tasks){

    for (int i=0; i< num_tasks; i++){
        printf("counts[%d] = %d\trowcounts[%d] = %d\tdisplacements[%d] = %d\n",
               i, data->counts[i], i, data->rowcounts[i],i, data->displacements[i]);
    }
    printf("\n");
}
/*
 * Aggiungo le righe fantasme alla sottomatrice e copio in una matrice che verrà usata per applicare le regole
 */
void mergeRowsAtMatrix(char *top_arr, char *bottom_arr, char *old_matrix, char *new_matrix, int rows, int cols){

    for (int i=0; i<rows; i++){
        for (int j=0; j<cols; j++) {
            if(i==0)
                new_matrix[i * cols + j] = top_arr[j];
            else if(i==rows-1)
                new_matrix[i * cols + j] = bottom_arr[j];
            else
                new_matrix[i * cols + j] = old_matrix[(i-1)*cols+j];
        }
    }
}
