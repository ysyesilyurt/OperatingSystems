#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/wait.h>

#define BUFFSIZE 250

void map(char *, int *, int *);
void reduce(char *, int *, int *, int *);

int main(int argc, const char* argv[]) {

    int i, j, N;
    char c;
    // WAIT!!

    if (argc == 3) {
        /* Executing Map Model Only */

        N = atoi(argv[1]);
        int pipes[N][2];
        for (i = 0; i < N; ++i)
            pipe(pipes[i]);
        for (i = 0; i < N; ++i) {
            if (fork()) { // Parent
                close(pipes[i][0]);
            }
            else { // Child
                for (j = 0; j < N; ++j)
                {
                    if (i != j)
                    {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                }
                int redPipe[1] = {-1};
                map(argv[2], pipes[i], redPipe);
            }
        }

        int lineNum = 0;
        char inp[BUFFSIZE];
        while(fgets(inp, BUFFSIZE, stdin) != NULL) {
            write(pipes[lineNum % N][1], inp, strlen(inp));
            lineNum++;
        }
        for (i = 0; i < N; ++i)
            close(pipes[i][1]);
        wait(&c);
    }
    else if (argc == 4) {
        /* Executing MapReduce Model */

        N = atoi(argv[1]);
        int mapperPipes[N][2];
        int mapRedPipes[N][2];
        int redRedPipes[N-1][2];
        for (i = 0; i < N; ++i) {
            pipe(mapperPipes[i]);
            pipe(mapRedPipes[i]);
            if (i != N-1)
                pipe(redRedPipes[i]);
        }
        for (i = 0; i < N; ++i) {
            if (fork()) { // Parent
                close(mapperPipes[i][0]);
                if (fork()) {
                    // Parent should close all the remaining pipes
                    close(mapRedPipes[i][0]);
                    close(mapRedPipes[i][1]);
                    close(redRedPipes[i][0]);
                    close(redRedPipes[i][1]);
                }
                else { // Reducer
                    for (j = 0; j < N; ++j)
                    {
                        if (i != j)
                        {
                            // Reducer should close all mapred pipes except ith mapred pipe
                            close(mapRedPipes[j][0]);
                            close(mapRedPipes[j][1]);
                            if (i != N-1)
                            {
                                // Also n-2 Reducers should close all redRedPipes pipes except ith redRedPipes pipe
                                close(redRedPipes[j][0]);
                                close(redRedPipes[j][1]);
                            }
                        }
                        // Should close all parent-to-mapper pipes
                        close(mapperPipes[j][0]);
                        close(mapperPipes[j][1]);
                    }
                    if (i == 0) {
                        int upRedPipe[1] = {-1};
                        reduce(argv[3], mapRedPipes[i], upRedPipe, redRedPipes[i]);
                    }
                    else if (i == N-1) {
                        int downRedPipe[1] = {-1};
                        reduce(argv[3], mapRedPipes[i], redRedPipes[i-1], downRedPipe);
                    }
                    else
                        reduce(argv[3], mapRedPipes[i], redRedPipes[i-1], redRedPipes[i]);
                }
            }
            else { // Mapper
                for (j = 0; j < N; ++j)
                {
                    // Mapper should close all pipes except ith mapper & mapred pipe
                    if (i != j)
                    {
                        close(mapperPipes[j][0]);
                        close(mapperPipes[j][1]);
                        close(mapRedPipes[j][0]);
                        close(mapRedPipes[j][1]);
                    }
                    // Should close all Red-to-Red pipes
                    close(redRedPipes[j][0]);
                    close(redRedPipes[j][1]);
                }
                map(argv[2], mapperPipes[i], mapRedPipes[i]);
            }
        }

        int lineNum = 0;
        char inp[BUFFSIZE];
        while(fgets(inp, BUFFSIZE, stdin) != NULL) {
            write(mapperPipes[lineNum % N][1], inp, strlen(inp));
            lineNum++;
        }
        for (i = 0; i < N; ++i)
            close(mapperPipes[i][1]);
        wait(&c);
    }
    else {
        fprintf(stderr, "You should provide either 3 or 4 arguments, Exiting...\n");
        return 1;
    }
    return 0;
}

void map(char * mapFunc, int * parentPipe, int * reducerPipe) {

    char dir[]="sample/src/";
    mapFunc = strcat(dir, mapFunc);

    if (reducerPipe[0] == -1) {
        /* Execute a Map Model */
        dup2(parentPipe[0],0);
        close(parentPipe[1]);
        close(parentPipe[0]);
        execl(mapFunc, NULL);
    }
    else {
        /* Execute a MapReduce Model */
        dup2(parentPipe[0],0);
        close(parentPipe[1]);
        close(parentPipe[0]);
        dup2(reducerPipe[1],1);
        close(reducerPipe[1]);
        close(reducerPipe[0]);
        execl(mapFunc, NULL);
    }
}

void reduce(char * reduceFunc, int * mapperPipe, int * upReducerPipe, int * downReducerPipe) {

    char dir[]="sample/src/";
    reduceFunc = strcat(dir, reduceFunc);

    dup2(mapperPipe[0],0);
    close(mapperPipe[1]);
    close(mapperPipe[0]);

    if ((upReducerPipe[0] == -1) && (downReducerPipe[0] == -1)) {
        // if there is only 1 reducer
        execl(reduceFunc, NULL);
    }
    else {
        if (upReducerPipe[0] == -1) {
            // if first reducer, then it has no pipe to its above
            dup2(downReducerPipe[1],1);
            close(downReducerPipe[1]);
            close(downReducerPipe[0]);
            execl(reduceFunc, NULL);
        }
        else if (downReducerPipe[0] == -1) {
            // if last reducer, then it has no pipe to its below
            dup2(upReducerPipe[0],2);
            close(upReducerPipe[1]);
            close(upReducerPipe[0]);
            execl(reduceFunc, NULL);
        }
        else {
            // normal reducer
            dup2(downReducerPipe[1],1);
            close(downReducerPipe[1]);
            close(downReducerPipe[0]);
            dup2(upReducerPipe[0],2);
            close(upReducerPipe[1]);
            close(upReducerPipe[0]);
            execl(reduceFunc, NULL);
        }
    }
}