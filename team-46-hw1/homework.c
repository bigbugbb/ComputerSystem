/*
 * file:        homework.c
 * description: Skeleton for homework 1
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, Jan. 2012
 * $Id: homework.c 500 2012-01-15 16:15:23Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uprog.h"

/***********************************/
/* Declarations for code in misc.c */
/***********************************/

typedef int *stack_ptr_t;
extern void init_memory(void);
extern void do_switch(stack_ptr_t *location_for_old_sp, stack_ptr_t new_value);
extern stack_ptr_t setup_stack(int *stack, void *func);
extern int get_term_input(char *buf, size_t len);
extern void init_terms(void);

extern void  *proc1;
extern void  *proc1_stack;
extern void  *proc2;
extern void  *proc2_stack;
extern void **vector;

int load_prog(const char* prog_name, void* location)
{
    FILE* fp = NULL;
    if ((fp = fopen(prog_name, "rb")) == NULL) {
        return -1;
    }
    int read = 0;
    while (!feof(fp)) {
        read = fread(location + read, 1, 4096, fp);
    }
    fclose(fp);
    
    return 0;
}

/***********************************************/
/********* Your code starts here ***************/
/***********************************************/

/*
 * Question 1.
 *
 * The micro-program q1prog.c has already been written, and uses the
 * 'print' micro-system-call (index 0 in the vector table) to print
 * out "Hello world".
 *
 * You'll need to write the (very simple) print() function below, and
 * then put a pointer to it in vector[0].
 *
 * Then you read the micro-program 'q1prog' into memory starting at
 * address 'proc1', and execute it, printing "Hello world".
 *
 */
void print(char *line)
{
    printf("%s", line);
}

void q1(void)
{
    /*
     * Your code goes here. Initialize the vector table, load the
     * code, and go.
     */
    // initialize the vector first
    vector[0] = print;
    
    // load the excecutable file and run
    int result = load_prog("q1prog", proc1);
    if (result == 0) {
        int (*prog) (void) = proc1;
        (*prog)();
    }
}


/*
 * Question 2.
 *
 * Add two more functions to the vector table:
 *   void readline(char *buf, int len) - read a line of input into 'buf'
 *   char *getarg(int i) - gets the i'th argument (see below)

 * Write a simple command line which prints a prompt and reads command
 * lines of the form 'cmd arg1 arg2 ...'. For each command line:
 *   - save arg1, arg2, ... in a location where they can be retrieved
 *     by 'getarg'
 *   - load and run the micro-program named by 'cmd'
 *   - if the command is "quit", then exit rather than running anything
 *
 * Note that this should be a general command line, allowing you to
 * execute arbitrary commands that you may not have written yet. You
 * are provided with a command that will work with this - 'q2prog',
 * which is a simple version of the 'grep' command.
 *
 * NOTE - your vector assignments have to mirror the ones in vector.s:
 *   0 = print
 *   1 = readline
 *   2 = getarg
 */
void readline(char *buf, int len) /* vector index = 1 */
{
    /*
     * Your code here.
     */
    char *result = fgets(buf, len, stdin);
    if(result != NULL){
        buf[strlen(buf)] = '\0';
    }
}

/* 
 * args is a pointer to the array that stores all the commands and
 * their arguments from the command line
 */
char **args = NULL;

char *getarg(int i)		/* vector index = 2 */
{
    /*
     * Your code here. 
     */
    if(i < 1024){
        return args[i+1];
    }
    return NULL;
}

/*
 * Note - see c-programming.pdf for sample code to split a line into
 * separate tokens. 
 */
void q2(void)
{
    /* Your code goes here */
    vector[0] = print;
    vector[1] = readline;
    vector[2] = getarg;
    
    char buf[1024] = {0};
    args = malloc(1024);

    while (1) {
        /* get a line */
        readline(buf, 1024);
        
        /* split it into words */
        
        /* strip the \n before tokenizing the input */
        strtok(buf, "\n");
        
        char *token = NULL;
        token = strtok(buf, " ");
        
        /* if zero words, continue */
        if(token == NULL){
            continue;
        }

        /* if first word is "quit", break */
        if(!strcmp(token, "quit")){
            break;
        }
        
        /* first word should be the command */
        int i = 0;
        args[i] = token;
        
        /* words after the first word are arguments */
        i++;
        for ( ; i < 1024; i ++) {
            if(token == NULL) {
                break;
            }
            token = strtok(NULL, " ");
            args[i] = token;
        }
	
        /* make sure 'getarg' can find the remaining words */
        /* load and run the command */
        int result = load_prog(args[0], proc1);
        if (result == 0) {
            int (*prog) (void) = proc1;
            (*prog)();
        }else{
            printf("%s\n", "invalid command");
        }
    }
    
    free(args);
    /*
     * Note that you should allow the user to load an arbitrary command,
     * and print an error if you can't find and load the command binary.
     */
}

/*
 * Question 3.
 *
 * Create two processes which switch back and forth.
 *
 * You will need to add another 3 functions to the table:
 *   void yield12(void) - save process 1, switch to process 2
 *   void yield21(void) - save process 2, switch to process 1
 *   void uexit(void) - return to original homework.c stack
 *
 * The code for this question will load 2 micro-programs, q3prog1 and
 * q3prog2, which are provided and merely consists of interleaved
 * calls to yield12() or yield21() and print(), finishing with uexit().
 *
 * Hints:
 * - Use setup_stack() to set up the stack for each process. It returns
 *   a stack pointer value which you can switch to.
 * - you need a global variable for each process to store its context
 *   (i.e. stack pointer)
 * - To start you use do_switch() to switch to the stack pointer for 
 *   process 1
 */

void* main_stack = 0;

void yield12(void)		/* vector index = 3 */
{
    do_switch((stack_ptr_t*) &proc1_stack, proc2_stack);
}

void yield21(void)		/* vector index = 4 */
{
    do_switch((stack_ptr_t*) &proc2_stack, proc1_stack);
}

void uexit(void)		/* vector index = 5 */
{
    do_switch(0, main_stack);
}

void q3(void)
{
    /* init vector */
    vector[0] = print;
    vector[3] = yield12;
    vector[4] = yield21;
    vector[5] = uexit;

    /* load q3prog1 into process 1 and q3prog2 into process 2 */
    if (load_prog("q3prog1", proc1) == -1) {
        printf("[q3]: can't find q3prog1 for loading!\n");
        return;
    }
    if (load_prog("q3prog2", proc2) == -1) {
        printf("[q3]: can't find q3prog2 for loading!\n");
        return;
    }

    /* setup stack for both process1 and process2 */
    proc1_stack = setup_stack((stack_ptr_t) proc1_stack, proc1);
    proc2_stack = setup_stack((stack_ptr_t) proc2_stack, proc2);

    /* then switch to process 1 */
    do_switch((stack_ptr_t*) &main_stack, proc1_stack);
}


/***********************************************/
/*********** Your code ends here ***************/
/***********************************************/
