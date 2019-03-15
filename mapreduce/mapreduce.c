#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFSIZE 1500

/*
 * C Implementation for MapReduce programming model
 *
 * Usage: ./mapreduce N MapperPath ReducerPath  -> Performs MapReduce Model
 *
 *  OR
 *        ./mapreduce N MapperPath -> Performs just the Map Model
 *
 *  MIT (c) 2019 Yavuz Selim Yesilyurt
 */


void map(char *, int *, int *, int);
void reduce(char *, int *, int *, int *, int);

int main(int argc, const char* argv[]) {

    int i, j, N;
    int lineNum = 0;
    char inp[BUFFSIZE];
    char c;

    if (argc == 3) {
        /* Executing Map Model Only */

        N = atoi(argv[1]);
        int pipes[N][2];
        for (i = 0; i < N; ++i) {
            pipe(pipes[i]);
        }
        for (i = 0; i < N; ++i) {
            if (fork()) {
                // Parent
                // Parent needs to keep pipes' write ends open
                close(pipes[i][0]);
            }
            else {
                // Child i ~ Mapper i
                for (j = 0; j < N; ++j)
                {
                    if (i != j)
                    {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                }
                int redPipe[1] = {-1};
                map(argv[2], pipes[i], redPipe, i);
            }
        }

        while(fgets(inp, BUFFSIZE, stdin) != NULL) {
            write(pipes[lineNum % N][1], inp, strlen(inp));
            lineNum++;
        }

        for (i = 0; i < N; ++i)
            close(pipes[i][1]);

        for (i = 0; i < N; ++i)
            wait(&c);
    }

    else if (argc == 4) {
        /* Executing MapReduce Model */

        N = atoi(argv[1]);
        int mapperPipes[N][2];
        int mapRedPipes[N][2];
        int redRedPipes[N][2];
        for (i = 0; i < N; ++i) {
            pipe(mapperPipes[i]);
            pipe(mapRedPipes[i]);
            if (i != N-1)
                pipe(redRedPipes[i]);
        }
        for (i = 0; i < N; ++i) {
            if (fork()) {
                // Parent
                if (fork()) {
                    // Parent needs to keep mapperPipes' write ends open
                    close(mapperPipes[i][0]);
                }
                else {
                    // Child i ~ Reducer i
                    // Reducer should close all the pipes except ith mapRed pipe and i-1th redRed pipe
                    for (j = 0; j < N; ++j) {
                        if (i != j) {
                            close(mapRedPipes[j][0]);
                            close(mapRedPipes[j][1]);
                            if ((j != N-1) && (j != i-1)) {
                                close(redRedPipes[j][0]);
                                close(redRedPipes[j][1]);
                            }
                        }
                        close(mapperPipes[j][0]);
                        close(mapperPipes[j][1]);
                    }
                    if (N == 1) {
                        // Only 1 Reducer exists
                        int upRedPipe[1] = {-1};
                        int downRedPipe[1] = {-1};
                        reduce(argv[3], mapRedPipes[i], upRedPipe, downRedPipe, i);
                    }
                    else {
                        if (i == 0) {
                            // First Reducer
                            int upRedPipe[1] = {-1};
                            reduce(argv[3], mapRedPipes[i], upRedPipe, redRedPipes[i], i);
                        }
                        else if (i == N-1) {
                            // Last Reducer
                            int downRedPipe[1] = {-1};
                            reduce(argv[3], mapRedPipes[i], redRedPipes[i-1], downRedPipe, i);
                        }
                        else
                            reduce(argv[3], mapRedPipes[i], redRedPipes[i-1], redRedPipes[i], i);
                    }
                }
            }
            else {
                // Child i ~ Mapper i
                // Mapper should close all pipes except ith mapper & mapRed pipe
                for (j = 0; j < N; ++j)
                {
                    if (i != j) {
                        close(mapperPipes[j][0]);
                        close(mapperPipes[j][1]);
                        close(mapRedPipes[j][0]);
                        close(mapRedPipes[j][1]);
                    }
                    if (j != N-1) {
                        close(redRedPipes[j][0]);
                        close(redRedPipes[j][1]);
                    }
                }
                map(argv[2], mapperPipes[i], mapRedPipes[i], i);
            }
        }

        // Parent should also close all the remaining pipes regarding mapper-reducer
        for (i = 0; i < N; ++i)
        {
            close(mapRedPipes[i][0]);
            close(mapRedPipes[i][1]);
            if (i != N-1) {
                close(redRedPipes[i][0]);
                close(redRedPipes[i][1]);
            }
        }

        while(fgets(inp, BUFFSIZE, stdin) != NULL) {
            write(mapperPipes[lineNum % N][1], inp, strlen(inp));
            lineNum++;
        }

        for (i = 0; i < N; ++i)
            close(mapperPipes[i][1]);

        for (i = 0; i < N; ++i)
            wait(&c);
    }

    else {
        fprintf(stderr, "You should provide either 3 or 4 arguments, Exiting...\n");
        fflush(stderr);
        return 1;
    }
    return 0;
}

/*
 * Mapping function for Mappers to execute their programs and
 * handle pipe communications between parent and reducer pipes gracefully.
 */
void map(char * mapProg, int * parentPipe, int * reducerPipe, int mapperID) {

    char param[BUFFSIZE];
    sprintf(param, "%d", mapperID);

    if (reducerPipe[0] == -1) {
        /* Execute a Map Model */

        dup2(parentPipe[0],0);
        close(parentPipe[1]);
        close(parentPipe[0]);
        execl(mapProg, mapProg, param, (char *) 0);
    }
    else {
        /* Execute a MapReduce Model */

        dup2(parentPipe[0],0);
        close(parentPipe[1]);
        close(parentPipe[0]);
        dup2(reducerPipe[1],1);
        close(reducerPipe[1]);
        close(reducerPipe[0]);
        execl(mapProg, mapProg, param, (char *) 0);
    }
}

/*
 * Reducing function for Mappers to execute their programs and
 * handle pipe communications between mapper and reducer pipes gracefully.
 */
void reduce(char * reduceProg, int * mapperPipe, int * upReducerPipe, int * downReducerPipe, int reducerID) {

    char param[BUFFSIZE];
    sprintf(param, "%d", reducerID);

    dup2(mapperPipe[0],0);
    close(mapperPipe[1]);
    close(mapperPipe[0]);

    if ((upReducerPipe[0] == -1) && (downReducerPipe[0] == -1)) {
        // Only 1 Reducer exists
        execl(reduceProg, reduceProg, param, (char *) 0);
    }
    else {
        if (upReducerPipe[0] == -1) {
            // First Reducer has no pipes to its above
            dup2(downReducerPipe[1],1);
            close(downReducerPipe[1]);
            close(downReducerPipe[0]);
            execl(reduceProg, reduceProg, param, (char *) 0);
        }
        else if (downReducerPipe[0] == -1) {
            // Last Reducer has no pipes to its below
            dup2(upReducerPipe[0],2);
            close(upReducerPipe[1]);
            close(upReducerPipe[0]);
            execl(reduceProg, reduceProg, param, (char *) 0);
        }
        else {
            // A Normal mid Reducer
            dup2(upReducerPipe[0],2);
            close(upReducerPipe[1]);
            close(upReducerPipe[0]);
            dup2(downReducerPipe[1],1);
            close(downReducerPipe[1]);
            close(downReducerPipe[0]);
            execl(reduceProg, reduceProg, param, (char *) 0);
        }
    }
}