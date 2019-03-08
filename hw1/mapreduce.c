#include <stdio.h>

int main(int argc, const char* argv[]) {

    if (argc == 3) {
        /* Executing Map Model */
    }
    else if (argc == 4) {
        /* Executing MapReduce Model */
    }
    else {
        fprintf(stderr, "You should either provide 3 or 4 arguments, Exiting...\n");
        return 1;
    }
    return 0;
}