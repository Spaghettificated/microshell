#include <stdio.h>
#include <dirent.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#define RED   "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW   "\x1B[33m"
#define BLUE   "\x1B[34m"
#define MAGENTA   "\x1B[35m"
#define CYAN   "\x1B[36m"
#define WHITE   "\x1B[37m"
#define NOCOLOR "\x1B[0m"
#define RED_BOLD   "\x1B[1;0;1m"
#define GREEN_BOLD   "\x1B[1;0;2m"
#define YELLOW_BOLD   "\x1B[1;0;3m"
#define BLUE_BOLD   "\x1B[1;0;4m"
#define MAGENTA_BOLD   "\x1B[1;0;5m"
#define CYAN_BOLD   "\x1B[36m"
#define WHITE_BOLD   "\x1B[1;0;7m"
#define NOCOLOR_BOLD "\x1B[1;0;0m"
#define BOLD "\x1B[1m"

void show_prompt(char *cursor){
    char hostname[256];
    if(gethostname(hostname, sizeof(hostname))==-1) {strcpy(hostname, "(null)");}
    printf(BOLD);
    printf(CYAN);
    printf("[%s@%s", getenv("USER"), hostname);
    printf(WHITE);
    printf(" %s",  cursor);
    printf(CYAN);
    printf("]$ ");
    printf(NOCOLOR);
}

int main(){
    show_prompt("xd");
    printf("\n");
    return 0;
}