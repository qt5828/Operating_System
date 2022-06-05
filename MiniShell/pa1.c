/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"
#include <sys/wait.h>

struct entry {
	struct list_head list;
	char *string;
	int index;
};

//function declaration
static int run_command(int nr_tokens, char *tokens[]);
static void append_history(char* const command);
void dump_history(void);
struct entry *find_history(char* num);
static int initialize(int argc, char* const argv[]);
static void finalize(int argc, char* const argv[]);
static int __process_command(char* command);
static void __print_prompt(void);
int is_pipe(char *tokens[]);

//for counting the number of history
int i = 0;

/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
LIST_HEAD(history);

/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
static int run_command(int nr_tokens, char *tokens[])
{

	//exit
	if (strcmp(tokens[0], "exit") == 0) return 0;
	
	//change working directory(20pts)
	if (strcmp(tokens[0], "cd") == 0) {

		//cd => home
		if(nr_tokens==1)
			chdir(getenv("HOME"));
		//cd [path] => path
		else{
			//cd ~ => home
			if(strcmp(tokens[1],"~")==0)
				chdir(getenv("HOME"));
			//=> path
			else
				chdir(tokens[1]);
		}
		return 1;
	}
	
	//keep the command history(50pts)	
	if (strcmp(tokens[0], "history") == 0) {
		
		//print history		
       		dump_history();
		
		return 1;
		
	}
	// ! 
	if (strcmp(tokens[0], "!") == 0) {
	
		struct entry *entry = (struct entry*)malloc(sizeof(*entry));
		entry = find_history(tokens[1]);

		char *buffer = (char*)malloc(sizeof(char)*(100));
		strcpy(buffer, entry->string);
		__process_command(buffer);
		
		return 1;
	}

	//Connect two processes with a pipe(100pts)
	int idx = is_pipe(tokens);	//check pipe and return to index of pipe
	if (idx != 0){		
		char** tokens1 = (char**)malloc(sizeof(char*)*(idx+1));	//before pipe
		char** tokens2 = (char**)malloc(sizeof(char*)*(nr_tokens-idx));	//after pipe

		//divide two tokens

		//first token
		int n;
		for(n = 0; n < idx; n++){
			tokens1[n] = (char*)malloc(sizeof(char)*100);
			strcpy(tokens1[n], tokens[n]);
		}
		tokens1[n] = NULL;
		//secound token(except pipe)
		int j = 0;
		for(++n ; n < nr_tokens; n++){
			tokens2[j] = (char*)malloc(sizeof(char)*100);
			strcpy(tokens2[j++], tokens[n]);
		}
		tokens2[j] = NULL;
		
		//make pipe
		int fd[2];	//fd[0]=read, fd[1]=write
		pipe(fd);
		
		//check error
		if(pipe(fd) == -1){
			perror("pipe");
			return -1;
		}
		
		//child 1
		//child가 pipe에 출력을 write
		if(!fork()){
			dup2(fd[1], 1); //출력이 pipe의 write에 들어가도록
			
			close(fd[0]); //read는 사용안하므로 close

			execvp(tokens1[0], tokens1);  //tokens1(| 전 명령) 실행
			return 1;
		}
		//child 2
		if(!fork()){
			dup2(fd[0], 0);  //입력받을 부분이 pipe의 read에 들어가도록
			close(fd[1]);  //write은 사용안하므로 close

			execvp(tokens2[0], tokens2);	//tokens2(| 뒤의 명령) 실행
			return 1;
		}
		
		//pipe 전체 close
		close(fd[0]);
		close(fd[1]);	
		
		//두개의 fork()로 생성된 child 종료 기다리기	
		wait(NULL);
		wait(NULL);
			
		return 1;
	}

	//Execute external commands(50pts)
	while(1){
		//parent
		if(fork()){
			wait(NULL);
			return 1;
		}
		//child
		if(execvp(tokens[0],tokens)<0){
			fprintf(stderr,"Unable to execute %s\n",tokens[0]);
			exit(0);
		}
	}

	return -EINVAL;
	
}


/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */

static void append_history(char * const command)
{
	
	//새로운 entry(history) 생성
	struct entry *new = (struct entry*)malloc(sizeof(*new));
	
	//새로운 entry의 string 메모리 할당
	new->string = (char*)malloc(strlen(command) + 1);
	strcpy(new->string, command);	//copy string
	new->index = i++;
	list_add(&(new->list),&history);	//connect list

}	

//print all history
//using dump_stack() from PA0
void dump_history()
{
	struct entry *entry;
        list_for_each_entry_reverse(entry, &history, list){
        	fprintf(stderr,"%2d: %s", entry->index, entry->string);
        }
}

//find history through own number
struct entry *find_history(char* num)
{
	struct entry *entry;
	
	list_for_each_entry_reverse(entry, &history, list){
		if(atoi(num) == entry->index)
			return entry;
	}
	return entry;
}

//check the *tokens[] is pipe
//if *tokens[] is pipe, return index of pipe
//else return 0
int is_pipe(char *tokens[])
{
	for(int i = 0; tokens[i] != NULL; i++)
		if(strcmp(tokens[i], "|") == 0)
			return i;
	return 0;
}

/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{

}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */

int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;

		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
