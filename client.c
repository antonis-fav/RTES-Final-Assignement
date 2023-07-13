// The following code is based on a client that connects to finnhub while using libwebsockets and it was created by a fellow colleague.
// You can find the source code of that client and extensive documentation here https://github.com/GohanDGeo/lws-finnhub-client.
// That source code was modified in order to implement the basic ideas and "mechanisms" of this source code here: 
// https://github.com/antonis-fav/Emdedded-Systems-Assignment-1. 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <libwebsockets.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
// Our input data come in serial order and their order must be preserved. So we cant have multiple producer threads. 
// The master thread is responsible for filling up the queue with transactions type structs 

#define QUEUESIZE 10
#define BUF_SIZE 10000
#define qn 8

pthread_t td[qn];
pthread_t candlestick_thread;

//Creating a mutex for every stock that we are subscribed to
pthread_mutex_t mutex_APPL;
pthread_mutex_t mutex_AMZN;
pthread_mutex_t mutex_BINANCE_BTCUSDT;
pthread_mutex_t mutex_IC_MARKETS_1;



 
int counter = 0;    
int buf_index = 0; //Index for the buffer buffer that holds all the incoming trasnactions
int minute = 0;
int avg_counter = 0;
int buffer_full = 0;

char candlestick_file[] = "candlestick.txt";
char candlestick_avg_15[]="Average_candlestick.txt";
char underscore[] = "_";
char backslash[]="/";
int after_15 = 0;

// Define colors for printing
#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define KBRN "\033[0;33m"
#define RESET "\033[0m"

//Declare the names of the stock market that you want to monitor
char *symbols[] = {"APPL\0", "AMZN\0", "BINANCE:BTCUSDT\0", "IC MARKETS:1\0"};

typedef struct{
    void*(*work)(void*);
    void* arg;
}workFunction;

typedef struct{
    workFunction buf[QUEUESIZE]; 
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
}queue;


typedef struct{
    float p;
    char s[20];
    unsigned long int ts;
    float v;
}transaction_data;

transaction_data transactions[BUF_SIZE];    //Setting up the  buffer that will hold the incoming transactions_data before they enter the queue

typedef struct{
    float init_p, final_p, max_p, min_p, total_vol, transaction_avg_price;
    unsigned long int transactions_number;
}candlestick;




queue* fifo;    //The queue in this case, will be a global struct variable
queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction pi);
void queueDel (queue *q, workFunction *out);
void* consumer();
void* routine(int args);
void* candlestickfunc();



// Variable that is =1 if the client should keep running, and =0 to close the client
static volatile int keepRunning = 1;

// Variable that is =1 if the client is connected, and =0 if not
static int connection_flag = 0;

// Variable that is =0 if the client should send messages to the server, and =1 otherwise
static int writeable_flag = 0;

// Function to handle the change of the keepRunning boolean
void intHandler(int dummy)
{
    printf("int handler activated\n");
    
    //Deallocating the mutexes from memory
    pthread_mutex_destroy(&mutex_AMZN);
    pthread_mutex_destroy(&mutex_APPL);
    pthread_mutex_destroy(&mutex_BINANCE_BTCUSDT);
    pthread_mutex_destroy(&mutex_IC_MARKETS_1);
    keepRunning = 0;
}

// The JSON paths/labels that we are interested in
static const char *const tok[] = {
    
    "data[].s", //symbol of the stock
    "data[].p", //price of the stock
    "data[].t", //timestrap :(milliseconds between the transaction and epoch)
    "data[].v", //volume of the transaction

};


// Callback function for the LEJP JSON Parser
static signed char
cb(struct lejp_ctx *ctx, char reason)
{
    // If the parsed JSON object is one we are interested in (so in the tok array), write to file
    if (reason & LEJP_FLAG_CB_IS_VALUE && (ctx->path_match > 0)) 
    {
        if(counter == 0){
            transactions[buf_index].p = atof(ctx->buf);
            counter++;
        }else if(counter == 1){
            strcpy(transactions[buf_index].s, ctx->buf);
            counter++;
        }else if(counter == 2){
            transactions[buf_index].ts = strtoul(ctx->buf, NULL, 10);
            counter++;
        }else if(counter == 3){
            transactions[buf_index].v = atof(ctx->buf);

            counter = 0;
            
            pthread_mutex_lock (fifo->mut);
            //printf("\ncheck 3\n");
            while (fifo->full) {
                printf ("queue FULL.\n");
                pthread_cond_wait (fifo->notFull, fifo->mut);
            }
            workFunction pi;

            pi.work = routine; // here the routine function is about writing the transaction's data into a file
            pi.arg = buf_index;
            queueAdd (fifo, pi);
            pthread_mutex_unlock (fifo->mut);
            pthread_cond_signal (fifo->notEmpty);
            // printf("main thread filled up the queue\n");

            buf_index = buf_index + 1;
            if (buf_index == BUF_SIZE){
                buf_index = 0;
                buffer_full = 1;
            }
        }else{
            printf("Counter got out of bounds exiting...\n");
        }
        //printf("\ncheck 2\n");

    }

    // If parsing is comlpeted, also write the UNIX timestamp in milliseconds of the current time
    if (reason == LEJPCB_COMPLETE)
    {
        // struct timeval tv;
        
        // gettimeofday(&tv, NULL);
    }
  
    return 0;
}

// Function used to "write" to the socket, so to send messages to the server
// @args:
// wsi_in        -> the websocket struct
// str          -> the message to write/send
// str_size_in  -> the length of the message
static int websocket_write_back(struct lws *wsi_in, char *str, int str_size_in)
{
    if (str == NULL || wsi_in == NULL){
        return -1;
    }
    // int m; 
    int n;
    int len;
    char *out = NULL;

    if (str_size_in < 1)
        len = strlen(str);
    else
        len = str_size_in;

    out = (char *)malloc(sizeof(char) * (LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
    //* setup the buffer*/
    memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);
    //* write out*/
    n = lws_write(wsi_in, out + LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);

    printf(KBLU "[websocket_write_back] %s\n" RESET, str);
    //* free the buffer*/
    free(out);

    return n;
}

// The websocket callback function      
static int ws_service_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason, void *user,
    void *in, size_t len)
{
    //printf("callback is called\n");
    // Switch-Case structure to check the reason for the callback
    switch (reason)
    {

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        printf(KYEL "[Main Service] Connect with server success.\n" RESET);
        for(int i=0; i<qn; i++){
            if(pthread_create(&td[i], NULL, consumer, NULL)!=0){
                printf("We cant create consumers threads exiting...\n");
                return 1;
            }
        }
        if(pthread_create(&candlestick_thread, NULL, candlestickfunc, NULL)!=0){
            printf("We cant create the candlestick thread exiting...\n");
            return 2;
        }
        printf("Create was ok\n");
        // Call the on writable callback, to send the subscribe messages to the server
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        printf(KRED "[Main Service] Connect with server error: %s.\n" RESET, in);
        // Set the flag to 0, to show that the connection was lost
        connection_flag = 0;
        break;

    case LWS_CALLBACK_CLOSED:
        printf(KYEL "[Main Service] LWS_CALLBACK_CLOSED\n" RESET);
        // Set the flag to 0, to show that the connection was lost
        connection_flag = 0;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:;
        // Incoming messages are handled here

        // UNCOMMENT for printing the message on the terminal
        printf(KCYN_L"[Main Service] Client received:%s\n"RESET, (char *)in);


        // Initialize a LEJP JSON parser, and pass it the incoming message
        char *msg = (char *)in;

        struct lejp_ctx ctx;
        lejp_construct(&ctx, cb, NULL, tok, LWS_ARRAY_SIZE(tok));
        int m = lejp_parse(&ctx, (uint8_t *)msg, strlen(msg));
        if (m < 0 && m != LEJP_CONTINUE)
        {
            lwsl_err("parse failed %d\n", m);
        }
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:

        // When writeable, send the server the desired trade symbols to subscribe to, if not already subscribed
        printf(KYEL "\n[Main Service] On writeable is called.\n" RESET);

        if (!writeable_flag)
        {
            char symb_arr[4][100] = {"APPL\0", "AMZN\0", "BINANCE:BTCUSDT\0", "IC MARKETS:1\0"};
            char str[50];
            for (int i = 0; i < 4; i++)
            {
                sprintf(str, "{\"type\":\"subscribe\",\"symbol\":\"%s\"}", symb_arr[i]);
                int len = strlen(str);
                websocket_write_back(wsi, str, len);
            }

            // Set the flag to 1, to show that the subscribe request have been sent
            writeable_flag = 1;
        }
        break;
    case LWS_CALLBACK_CLIENT_CLOSED:

        // If the client is closed for some reason, set the connection and writeable flags to 0,
        // so a connection can be re-established
        printf(KYEL "\n[Main Service] Client closed %s.\n" RESET, in);
        connection_flag = 0;
        writeable_flag = 0;

        break;
    default:
        break;
    }

    return 0;
}

// Protocol to be used with the websocket callback
static struct lws_protocols protocols[] =
    {
        {
            "trade_protocol",
            ws_service_callback,
        },
        {NULL, NULL, 0, 0} /* terminator */
};


// Main function
int main(void)
{
    // Set intHandle to handle the SIGINT signal
    // (Used for terminating the client)
    signal(SIGINT, intHandler);
    
    pthread_mutex_init(&mutex_AMZN, NULL);
    pthread_mutex_init(&mutex_APPL, NULL);
    pthread_mutex_init(&mutex_BINANCE_BTCUSDT, NULL);
    pthread_mutex_init(&mutex_IC_MARKETS_1, NULL);

    
    struct stat sb;
    int i = 0;
    for (i = 0; i < 4; i++)
    {
        if (!(stat(symbols[i], &sb) == 0 && S_ISDIR(sb.st_mode)))
        {
            mkdir(symbols[i], 0755);
        }
    }

    //printf("after the mkdir\n");
    fifo = queueInit();
    

    // Set the LWS and its context
    struct lws_context *context = NULL;
    struct lws_context_creation_info info;
    struct lws *wsi = NULL;
    struct lws_protocols protocol;

    memset(&info, 0, sizeof info);


    // Set the context of the websocket
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    // Set the Finnhub url
    char *api_key = "YOUR_API_KEY"; // PLACE YOUR API KEY HERE!
    if (strlen(api_key) == 0)
    {
        printf(" API KEY NOT PROVIDED!\n");
        return -1;
    }

    // Create the websocket context
    context = lws_create_context(&info);
    printf(KGRN "[Main] context created.\n" RESET);

    if (context == NULL)
    {
        printf(KRED "[Main] context is NULL.\n" RESET);
        return -1;
    }

    // Set up variables for the url
    char inputURL[300];

    sprintf(inputURL, "wss://ws.finnhub.io/?token=%s", api_key);
    const char *urlProtocol, *urlTempPath;
    char urlPath[300];

    struct lws_client_connect_info clientConnectionInfo;
    memset(&clientConnectionInfo, 0, sizeof(clientConnectionInfo));

    // Set the context for the client connection
    clientConnectionInfo.context = context;
    
    // Parse the url
    if (lws_parse_uri(inputURL, &urlProtocol, &clientConnectionInfo.address,
                      &clientConnectionInfo.port, &urlTempPath))
    {
        printf("Couldn't parse URL\n");
    }

    urlPath[0] = '/';
    strncpy(urlPath + 1, urlTempPath, sizeof(urlPath) - 2);
    urlPath[sizeof(urlPath) - 1] = '\0';

    // While a kill signal is not sent (ctrl+c), keep running
    while (keepRunning)
    {
        // If the websocket is not connected, connect
        if (!connection_flag || !wsi)
        {
            // Set the client information

            connection_flag = 1;
            clientConnectionInfo.port = 443;
            clientConnectionInfo.path = urlPath;
            clientConnectionInfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

            clientConnectionInfo.host = clientConnectionInfo.address;
            clientConnectionInfo.origin = clientConnectionInfo.address;
            clientConnectionInfo.ietf_version_or_minus_one = -1;
            clientConnectionInfo.protocol = protocols[0].name;

            printf(KGRN "Connecting to %s://%s:%d%s \n\n" RESET, urlProtocol,
                   clientConnectionInfo.address, clientConnectionInfo.port, urlPath);

            wsi = lws_client_connect_via_info(&clientConnectionInfo);
            if (wsi == NULL)
            {
                printf(KRED "[Main] wsi create error.\n" RESET);
                return -1;
            }

            printf(KGRN "[Main] wsi creation success.\n" RESET);
        }

        // Service websocket activity
        lws_service(context, 0);
    }

    printf(KRED "\n[Main] Closing client\n" RESET);
    lws_context_destroy(context);
    return 0;
}

queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, workFunction pi)
{

  q->buf[q->tail] = pi;

  q->tail++;

  if (q->tail == QUEUESIZE){
    q->tail = 0;
  }

  if (q->tail == q->head){
    q->full = 1;
  }
  q->empty = 0;
  return;

}

void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];
  q->head++;
  if (q->head == QUEUESIZE){
    q->head = 0;
  }
  if (q->head == q->tail){
    q->empty = 1;
  }
  q->full = 0;

  return;
}

void *consumer (){
  while(1){

    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      printf ("consumer: queue EMPTY.\n");
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    } 
    printf("from consumer got the lock\n");
    workFunction out;
    queueDel (fifo, &out);

    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);

    int p = out.arg;

    if(strcmp (transactions[p].s, symbols[0]) ==0){
        pthread_mutex_lock(&mutex_APPL);
        out.work(out.arg);
        pthread_mutex_unlock(&mutex_APPL);
    }
    if(strcmp (transactions[p].s, symbols[1]) ==0){
        pthread_mutex_lock(&mutex_AMZN);
        out.work(out.arg);
        pthread_mutex_unlock(&mutex_AMZN);
    }
    if(strcmp (transactions[p].s, symbols[2]) ==0){
        pthread_mutex_lock(&mutex_BINANCE_BTCUSDT);
        out.work(out.arg);
        pthread_mutex_unlock(&mutex_BINANCE_BTCUSDT);
    }
    if(strcmp (transactions[p].s, symbols[3]) ==0){
        pthread_mutex_lock(&mutex_IC_MARKETS_1);
        out.work(out.arg);
        pthread_mutex_unlock(&mutex_IC_MARKETS_1);
    }
  }
  return(NULL);
}



void* routine(int args){

    int r_c = args;
    printf("r_c = %d\n", r_c);

    transaction_data td = transactions[r_c];

    char trading_file[50] = "";
    char timestrap_file[50] = "";
    strcat(trading_file, td.s);
    strcat(trading_file, backslash);
    strcat(trading_file, td.s);
    strcat(trading_file, underscore);
    strcat(trading_file, "trading_data.txt");
    
    struct timeval endwtime;
    
    printf("%f\n", td.p);
    printf("%s\n", td.s);
    printf("%lu\n", td.ts);
    printf("%f\n", td.v);
    FILE* fp3 = fopen(trading_file, "a");
    
    
    fprintf(fp3, "Price: %f      Symbol: %s     Timestrap: %lu     Volume: %f \n", td.p, td.s, td.ts, td.v);
    fclose(fp3);
    // printf("ok\n");
    gettimeofday(&endwtime, NULL);
    unsigned long  milliseconds_end =
        (unsigned long)(endwtime.tv_sec) * 1000 +
         (unsigned long)(endwtime.tv_usec) / 1000;

    
    strcat(timestrap_file, td.s);
    strcat(timestrap_file, backslash);
    strcat(timestrap_file, td.s);
    strcat(timestrap_file, underscore);
    strcat(timestrap_file, "timestraps.txt");

    FILE* fp4 = fopen(timestrap_file, "a");
    fprintf(fp4, "%lu, %lu \n", td.ts, milliseconds_end); // We print only the timestraps, because later a python script will use this timestraps.txt fil
    fclose(fp4);
    printf("we finished the 1st trhread\n");
}

void* candlestickfunc(){
    // We monitor 4 stocks
    // We produce the average candlestick of the last 15 minutes
    // The candlestick of each stock is computated every 1 minute, so we need a candlestick type array, size [4][15]
    candlestick candlestick_arr[15][4]; 

    unsigned long int candle_counter = 0; 
    transaction_data temp; 
    transaction_data final_data[4];
    int minute =0;  
    int j=0;
    int i=0;
    float price_avg_15m = 0;
    float total_vol_15m = 0;
    while(1){
        
        for(j=0; j<4; j++){
        candlestick_arr[minute][j].init_p = 0;
        candlestick_arr[minute][j].final_p = 0;
        candlestick_arr[minute][j].max_p = 0;
        candlestick_arr[minute][j].min_p = 100000000;
        candlestick_arr[minute][j].total_vol = 0;
        candlestick_arr[minute][j].transaction_avg_price = 0;
        candlestick_arr[minute][j].transactions_number = 0;
        final_data[j].p =0;
        }
        printf("candle ok\n");
    
        sleep(60); // We are sampling our data for the candlestick every 1 minute
        struct timeval wtime;
        gettimeofday(&wtime, NULL);
        unsigned long  milliseconds_candle =
        (unsigned long)(wtime.tv_sec) * 1000 +
         (unsigned long)(wtime.tv_usec) / 1000;


        if(buffer_full == 1){
            for(candle_counter=0; candle_counter < BUF_SIZE; candle_counter++){

                temp = transactions[candle_counter];

                if(milliseconds_candle - transactions[candle_counter].ts < 60000){
                    for(j=0; j<4; j++){

                        if(strcmp(temp.s, symbols[j]) == 0){ 
                            if(candlestick_arr[minute][j].init_p == 0){
                                candlestick_arr[minute][j].init_p = temp.p;
                            }
                            if(temp.p < candlestick_arr[minute][j].min_p){
                                candlestick_arr[minute][j].min_p =temp.p;
                            }else if(candlestick_arr[minute][j].min_p == 1000000000){
                                //candlestick_arr[minute][j].min_p = 0;
                            }
                            if(temp.p > candlestick_arr[minute][j].max_p){
                                candlestick_arr[minute][j].max_p = temp.p;
                            }
                            candlestick_arr[minute][j].total_vol =  candlestick_arr[minute][j].total_vol + temp.v;
                            candlestick_arr[minute][j].transactions_number++;
                            candlestick_arr[minute][j].transaction_avg_price = candlestick_arr[minute][j].transaction_avg_price + temp.p;
                            final_data[j] = temp;
                        }
                    }
                }
            }
        }else{
            candle_counter = 0;
            int k = 0 ;
            k = buf_index;
            while(candle_counter < k){

                temp = transactions[candle_counter];
                if(milliseconds_candle - transactions[candle_counter].ts < 60000){
                    for(j=0; j<4; j++){

                        if(strcmp(temp.s, symbols[j]) == 0){ 
                            if(candlestick_arr[minute][j].init_p == 0){
                                candlestick_arr[minute][j].init_p = temp.p;
                            }
                            if(temp.p < candlestick_arr[minute][j].min_p){
                                candlestick_arr[minute][j].min_p =temp.p;
                            }else if(candlestick_arr[minute][j].min_p == 1000000000){
                                //candlestick_arr[minute][j].min_p = 0;
                            }
                            if(temp.p > candlestick_arr[minute][j].max_p){
                                candlestick_arr[minute][j].max_p = temp.p;
                            }
                            candlestick_arr[minute][j].total_vol =  candlestick_arr[minute][j].total_vol + temp.v;
                            candlestick_arr[minute][j].transactions_number++;
                            candlestick_arr[minute][j].transaction_avg_price = candlestick_arr[minute][j].transaction_avg_price + temp.p;
                            final_data[j] = temp;
                        }
                    }
                }
                candle_counter++;
                if (candle_counter == BUF_SIZE){
                    candle_counter = 0;
                }
            }
        }
        
        for(j=0; j<4; j++){
            candlestick_arr[minute][j].final_p = final_data[j].p;
            if(candlestick_arr[minute][j].transactions_number != 0){
                candlestick_arr[minute][j].transaction_avg_price = candlestick_arr[minute][j].transaction_avg_price / candlestick_arr[minute][j].transactions_number;
            }else{
                candlestick_arr[minute][j].transaction_avg_price = 0;
                candlestick_arr[minute][j].min_p = 0;
            }
            char candle_info[50]="";
            strcat(candle_info, symbols[j]);
            strcat(candle_info, backslash);
            strcat(candle_info, symbols[j]);
            strcat(candle_info, underscore);
            strcat(candle_info, candlestick_file);
            FILE* fp = fopen(candle_info, "a");
            fprintf(fp,"Initial price: %f,      Last price: %f,      Min price: %f,      Max price: %f,      Total volume:%f\n",
                    candlestick_arr[minute][j].init_p,
                    candlestick_arr[minute][j].final_p,
                    candlestick_arr[minute][j].min_p,
                    candlestick_arr[minute][j].max_p,
                    candlestick_arr[minute][j].total_vol);
            fclose(fp);

        }
            
        
        if(minute == 14){ // we mark the 1st time we surpass the time frame of 15 minutes, in order to start printing the average candlesticks
            after_15 = 1;
        }
        if(after_15 == 1){

            for(j=0; j<4; j++){
                char candle_avg[50]="";
                strcat(candle_avg, symbols[j]);
                strcat(candle_avg, backslash);
                strcat(candle_avg, symbols[j]);
                strcat(candle_avg, underscore);
                strcat(candle_avg, candlestick_avg_15);

                price_avg_15m = 0;
                total_vol_15m = 0;
                avg_counter = 0;
                for(i=0; i<15; i++){
                    if(candlestick_arr[i][j].transaction_avg_price != 0){
                        price_avg_15m += candlestick_arr[i][j].transaction_avg_price;
                        avg_counter++;
                    }
                    total_vol_15m += candlestick_arr[i][j].total_vol;
                }
                if(avg_counter!=0){
                    price_avg_15m = price_avg_15m / avg_counter; // The mean average price per transacion of 15 minutes
                }else{
                    price_avg_15m = 0;
                }
                FILE* fp2 = fopen(candle_avg, "a");
                fprintf(fp2, "Average price per transaction (last 15m): %f.        The total volume (last 15m): %f \n",
                        price_avg_15m,
                        total_vol_15m
                );
                fclose(fp2);
            }
        }
        minute++;
        // reseting the counter minute when it reaches 15
        if(minute == 15){
            minute = 0;
        }
    }

}
    
