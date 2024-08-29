## Reproducing Mismatched Results Using RDS Proxy

### Objective
The objective of this experiment was to investigate the behavior of a PostgreSQL RDS instance accessed via an RDS Proxy, particularly to observe if there were any discrepancies when executing concurrent queries under high load.

### Setup

1. **Environment:**
   - **Database:** PostgreSQL RDS Aurora with version 15, running on a `db.t3.medium` instance with 2 vCPUs and 4 GB RAM.
   - **Proxy:** An RDS Proxy was used to route the connections to the database.
   - **Max Connections:** The theoretical maximum number of connections was calculated as approximately 450, based on the instance's memory.

2. **Software:**
   - **C Program:** A custom C program was developed to simulate high-load concurrent query execution. The program used the `libpq` library to interact with the PostgreSQL database.
   - **Threads:** The program created 600 threads to simulate 600 concurrent clients, each executing a large number of queries.

3. **Query Execution:**
   - Each thread generated random alphanumeric strings to use as parameters in SQL queries.
   - The program executed a parameterized query that compared a randomly generated `parameterizedId` with a `literalId` within the SQL query.
   - The goal was to check if `parameterizedId`, `literalId`, and the provided `paramValue` were consistent. If any discrepancy was detected (i.e., the values did not match), it would be reported as a mismatch.

4. **Error Handling:**
   - The program was designed to ignore certain expected errors like "no connection to the server" and "SSL connection has been closed unexpectedly," which might occur under high load or network disruptions.

5. **Execution:**
   - The program was executed with the following command:
   ```bash
    clang -I/opt/homebrew/opt/libpq/include -L/opt/homebrew/opt/libpq/lib -lpq main-extended-parallel.c -o pg_connect && ./pg_connect 2>&1 | tee "pg_connect_$(date '+%Y-%m-%d_%H-%M-%S').log"
    ```
   - Output was logged to a file with a timestamp, and any mismatches were printed both to the console and the log file.

6. **Results**
   - Mismatches Detected:
    The program successfully detected multiple mismatches between the parameterizedId and literalId. This indicates that under certain conditions, the RDS Proxy might be causing inconsistencies when routing queries.
    A total of 29 mismatches were reported during the test run.
