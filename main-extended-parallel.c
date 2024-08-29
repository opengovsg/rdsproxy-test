#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <sys/time.h>

// Assume necessary imports and constants are defined...

// Struct to hold thread information
typedef struct {
    int thread_id;
    const char *conninfo;
    int nqueries;
    int differentCount;
} ThreadData;

// Function to generate a random alphanumeric string of a given length
void generate_random_id(char *id, size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    for (size_t i = 0; i < length - 1; ++i) {
        id[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    id[length - 1] = '\0';
}

// Function to execute a query
int execute_query(PGconn *conn, const char *paramValue) {
    char query[512];
    
    // Construct the query string with the literal ID injected
    snprintf(query, sizeof(query),
             "select $1 as \"parameterizedId\", '%s' as \"literalId\" where ('true' = 'true' or $2 = $3)",
             paramValue);

    const char *paramValues[3];
    paramValues[0] = paramValue;
    paramValues[1] = paramValue;
    paramValues[2] = paramValue;

    PGresult *res = PQexecParams(conn,
                                 query,            // The dynamically constructed query
                                 3,                // Number of parameters
                                 NULL,             // Parameter types (NULL means infer from values)
                                 paramValues,      // Parameter values
                                 NULL,             // Lengths of binary values (not needed for text)
                                 NULL,             // Binary format flags (0 means text)
                                 0);               // Result format (0 means text)

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        // ignore "no connection to the server" error and "SSL connection has been closed unexpectedly" error
        if (strcmp(PQerrorMessage(conn), "no connection to the server\n") != 0 && strcmp(PQerrorMessage(conn), "SSL connection has been closed unexpectedly\n") != 0) {
            fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
        }
        PQclear(res);
        return 0;
    }

    const char *returnedId = PQgetvalue(res, 0, 0);
    const char *literalId = PQgetvalue(res, 0, 1);
    // int result = strcmp(returnedId, paramValue) == 0 ? 0 : 1; // 0 if same, 1 if different
    // if returnedId != paramValue or returnedId != literalId, or literalId != paramValue, return 1

    int result = 0;
    if (strcmp(returnedId, paramValue) != 0 || strcmp(returnedId, literalId) != 0 || strcmp(literalId, paramValue) != 0) {
        result = 1;
    }

    if (result != 0) {
        const char *literalId = PQgetvalue(res, 0, 1);
        printf("MISMATCH DETECTED!\n");
        printf("parameterizedId: %s\n", returnedId);
        printf("literalId: %s\n", literalId);
    }

    PQclear(res);
    return result;
}

// Thread function
void *thread_function(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    PGconn *conn = PQconnectdb(data->conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        pthread_exit(NULL);
    }

    srand(time(NULL) + data->thread_id); // Unique seed for each thread
    char randomId[23]; // Assuming a random ID of 22 characters plus null terminator

    for (int i = 0; i < data->nqueries; i++) {
        generate_random_id(randomId, sizeof(randomId));
        data->differentCount += execute_query(conn, randomId);
        if (i  % 500 == 0) {
            printf("Thread %d: %d queries completed\n", data->thread_id, i);
        }
    }

    PQfinish(conn);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    int nthreads = 600; // Number of threads
    int nqueries = 1000000 / nthreads; // Queries per thread

    // int nthreads = 1;
    // int nqueries = 1;

    pthread_t threads[nthreads];
    ThreadData thread_data[nthreads];
    int total_differentCount = 0;

    // local connection
    // const char *conninfo = "host=localhost port=5432 dbname=activesg user=root password=root";

    // rds proxy endpoint
    // read/write
    const char *conninfo = "host=proxy-1724929386346-xianxiang-test.proxy-c1fgfjfkekqh.ap-southeast-1.rds.amazonaws.com port=5432 dbname=XXX user=XXX password=XXX sslmode=require";

    // using rds proxy with aurora postgers version 15, db.t3.medium -> 2 vcpu, 4gb ram
    // max connections LEAST({DBInstanceClassMemory/9531392}, 5000) -> (4 * 1024 * 1024 * 1024) / 9531392 = 450.6

    for (int i = 0; i < nthreads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].conninfo = conninfo;
        thread_data[i].nqueries = nqueries;
        thread_data[i].differentCount = 0;

        pthread_create(&threads[i], NULL, thread_function, &thread_data[i]);
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
        total_differentCount += thread_data[i].differentCount;
    }

    printf("Total different results: %d\n", total_differentCount);

    return 0;
}
