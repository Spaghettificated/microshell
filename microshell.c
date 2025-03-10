#define _GNU_SOURCE // DT_DIR, DT_REG, DT_..
#include <stdio.h>
#include <dirent.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h> // mkdir; check if file is dir / file / exist
#include <errno.h> // errors

#include <fcntl.h> // O_RDONLY


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

#define STR_BUFOR_SIZE 256
#define ARG_BUFOR_SIZE 64

typedef struct commandArgs{
    char *name;
    char *argv[ARG_BUFOR_SIZE];
    int argc;
    char *redirects[ARG_BUFOR_SIZE];
}commandArgs;

typedef struct streams{
    int in;
    int out;
    int err;
}streams;


// #region typing_fields
typedef struct typingField{
    char *start;
    char *cursor;
    char *end;
    int backtrack;
}typingField;

int bound_dist(char *ptr, typingField *field){
    if(ptr > field->end) return ptr - field->end;
    if(ptr < field->start) return ptr - field->start;
    return 0;
}
char *bounds_snap(char *ptr, typingField *field){
    if(ptr > field->end) return field->end;
    if(ptr < field->start) return field->start;
    return ptr;
}
void insertc(char c, typingField *field){
    *field->cursor = c;
    if (c=='\0'){
        printf(" \b");
        field->end = field->cursor;
    }
    else{
        printf("%c", c);
        field->cursor++;
    }
}
int move_cursor(int amount, struct typingField *field){
    char *target = field->cursor + amount;
    char *message;
    if(amount>0){
        message = "\e[C";
        target = target < field->end ? target : field->end;
    }
    else{
        message = "\b";
        target = target > field->start ? target : field->start;
    }
    amount = target - field->cursor;
    for (int i = 0; i < abs(amount); i++) {
        printf(message);
    }
    field->cursor = target; 
    return amount;
}
int set_cursor(char *new_cursor, typingField *field){
    new_cursor = bounds_snap(new_cursor, field);
    return move_cursor(new_cursor - field->cursor, field);
}
void typec(char c, typingField *field){
    char c1;
    char *from = field->cursor;
    while(field->cursor <= field->end){
        c1 = *field->cursor;
        insertc(c, field);
        c = c1;
    }
    insertc(c1, field);
    set_cursor(from+1, field);
}
void types(char *s, typingField *field){
    char *ptr = s;
    while (*ptr != '\0'){
        typec(*ptr, field);
        ptr++;
    }
    
}
void tail_to_cursor(char* tail_pos, typingField *field){
    char *to = field->cursor;
    do{
        insertc(*tail_pos, field);
    }while (*(++tail_pos) != '\0');
    int diff = 0;
    while (field->cursor < tail_pos){
        insertc(' ', field);
        diff++;
    }    
    // set_cursor(to, field);
    move_cursor(-diff, field);
    insertc('\0', field);
    set_cursor(to, field);
}
void backspace(typingField *field){
    if(move_cursor(-1, field) == -1){
        tail_to_cursor(field->cursor+1, field);
    }
}
void deletec(typingField *field){
    tail_to_cursor(field->cursor+1, field);
}
char *token_left(char* start, typingField *field){
    while( (--start) != field->start){
        if(*(start-1) == ' ' && *start != ' ' && *start != '\0') return start;
    }
    return start;
}
char *token_right(char* start, typingField *field){
    while( (++start) != field->end){
        if(*(start+1) == ' ' && *start != ' ' && *start != '\0') return start;
    }
    return start;
}
void clear_field(typingField *field){
    // *field->start = '\0';
    // field->cursor = field->start;
    // field->end = field->start;
    set_cursor(field->start, field);
    tail_to_cursor(field->end, field);
}
// #endregion

// #region paths
int check_in_path(char *path, char *name){
    struct dirent *entry; // https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    DIR *dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (!strcmp(entry->d_name, name)){
                closedir(dir);
                return entry->d_type;
            }
        }
        closedir(dir);
    }
    return -1;
}
int check_path(char *path){
    if (path[0] != '/'){
        printf("wtff? somwhere the path is is just plain wrong... relative brrr"); return -1;
    }

    char *div = strrchr(path,'/');
    if(div[1] == '\0')
        return DT_DIR;
    int type;
    if (div == path){
        type = check_in_path("/", div+1);
    }
    else{
        *div = '\0';
        type = check_in_path(path, div+1);
        *div = '/';
    }
    return type;
}
int move_path(char *from, char *path){
    if(!strncmp(path,"/",1)){
        strcpy(from, "/");
        path++;
    }
    else if (!strcmp(path,"~") || !strncmp(path,"~/",2)){
        strcpy(from, getenv("HOME"));
        path++;
    }
    char *token = strtok(path,"/");
    while(token != NULL){
        if(!strcmp(token, ".")){
            // continue;
        }
        else if(!strcmp(token, "..")){
            char *slash = strrchr(from, '/');
            if (slash == from)
                slash[1] = '\0';
            else if (slash != NULL)
                slash[0] = '\0';
            // continue;
        }
        else{
            // if( !check_in_path(from, token) ){
            //     return 1;
            // }
            // else{
                char *to_move_end = strchr(from, '\0');
                if (to_move_end[-1] != '/')
                    strcat(from, "/");
                strcat(from, token);
            //     // return move_path(from, path);
            // }
        }

        token = strtok(NULL,"/");
    }
    return 0;
}
// #endregion

// #region flags
typedef struct flag_info{
    int flags;
    int argc;
    int argid[ARG_BUFOR_SIZE];
}flag_info;
// 0 - not flag, 1 - short flag, 2 - long flag
int flag_type(char *str){
    if (str[0] == '-'){
        if (str[1] == '-'){
            return 2;
        }
        return 1;
    }
    return 0;
}
int is_flag(char *str, int type, char c_flag, char* s_flag){
    // if (type == 1 && c_flag != NULL && strchr(str, c_flag) != NULL){
    if (type == 1 && strchr(str, c_flag) != NULL){
        return 1;
    }
    if (type == 2 && s_flag != NULL && strcmp(str+2, s_flag)){
        return 1;
    }
    return 0;
}
int get_flags(char *str, char *c_flags, char *s_flags[]){
    int type = flag_type(str);
    if (type == 0) return 0;
    int n = type==1 ? strlen(str)-1 : 1;
    int out = 0;
    for(int i = 0; c_flags[i] != '\0'; i++){
        if (n<=0) break;
        if (
            (type == 1 && strchr(str+1, c_flags[i]) != NULL)
            || (s_flags != NULL && s_flags[i] != NULL 
                && type == 2 && !strcmp(s_flags[i], str+2)) 
        ){
            out |= 1<<i;
            n--;
        }
    }
    out = n<=0 ? out : 0;
    return out;
}
flag_info get_flag_info(commandArgs command, char *c_flags, char *s_flags[]){
    flag_info out;
    out.flags = 0;
    out.argc = 0;
    for(int i = 1; i < command.argc; i++){
        int f = get_flags(command.argv[i], c_flags, s_flags);
        out.flags |= f;
        if (f==0){
            out.argid[out.argc] = i;
            out.argc++;
        }
    }
    return out;
}
int has_flag(int flags, int n){
    return flags & (1<<n);
}
// #endregion

// #region built_ins
void help(commandArgs command, char *cursor){
    fprintf(stdout, "Program powłoki microshell\n");
    fprintf(stdout, "funkcje programu\n");
    fprintf(stdout, "- obługa następujących komend:\n");
    fprintf(stdout, "  - help\n");
    fprintf(stdout, "  - ls [-a|--all] [paths]\n");
    fprintf(stdout, "  - cd [paths]\n");
    fprintf(stdout, "  - echo [messages]\n");
    fprintf(stdout, "  - cat [paths]\n");
    fprintf(stdout, "  - mkdir [paths]\n");
    fprintf(stdout, "- wyświetlanie kolorowego znaku zachęty\n");
    fprintf(stdout, "- nawigacja - strzałki, ctrl + strzałki\n");
    fprintf(stdout, "- historia\n");
    fprintf(stdout, "- uruchamianie programów z PATH\n");
}
void ls_once(char *path, int flags){
    DIR *dir;
    struct dirent *entry; // https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (!has_flag(flags, 0) && entry->d_name[0]=='.'){ // -a flag
                continue;
            }
            fprintf(stdout, "%s ", entry->d_name);
        }
        closedir(dir);
    }
    fprintf(stdout,"\n");
}
void ls(commandArgs command, char *cursor){

    flag_info flags = get_flag_info(command, "a", (char*[]){"all",});
    int n = flags.argc;

    if (n==0){
        ls_once(cursor, flags.flags);
        return;
    }

    for(int i = 0; i < n ; i++){
        int j = flags.argid[i];
        char path[STR_BUFOR_SIZE];
        strcpy(path, cursor);
        move_path(path, command.argv[j]);
            
        if (check_path(path) != DT_DIR){
            fprintf(stderr, "path error: %s\n", path);
            if(n>1 && i != n-1) fprintf(stderr, "\n");
            continue;
        }
        if(n>1) fprintf(stdout, "%s:\n", command.argv[j]);
        ls_once(path, flags.flags);
        if(n>1 && i != n-1) fprintf(stdout, "\n");
    }
}
int cd(commandArgs command, char *cursor){
    if(command.argc<=1){
        command.argc++;
        command.argv[1] = "~";
    }
    char *path = command.argv[1];

    char new_cursor[STR_BUFOR_SIZE];
    if (path[0] != '/'){
        strcpy(new_cursor, cursor);
    }
    move_path(new_cursor, path);
    if (check_path(new_cursor) == DT_DIR){
        strcpy(cursor, new_cursor);
        return 0;
    }
    // else{
    //     fprintf(streams.err, "cd: path [%s] not found [%s] -> [%s]\n", new_cursor, cursor, path);
    //     return 1;
    // }
}
int echo(commandArgs command, char *cursor){
    for(int i = 1; i<command.argc; i++){
        char *message = command.argv[i];
        // printf("\t\t> echo: '%s' to %d\n", message, streams.out);
        printf("%s\n", message);
        printf("\0");
    }
    return 0;
}
int cat(commandArgs command, char *cursor){
    // printf("\t\t> cat from %d to %d: \n", in, out);
    if(command.argc <= 1){
        char buffer[64];
        while(fgets(buffer, sizeof(buffer), stdin) != NULL){
            // printf("\t\t %s", buffer);
            printf(buffer);
        }
        return 0;
    }
    else{
        char path[STR_BUFOR_SIZE];
        char buffer[256];
        for(int i = 1; i < command.argc; i++){
            strcpy(path, cursor);
            move_path(path, command.argv[i]);
            if(check_path(path) == DT_REG){
                int fd = open(path, O_RDONLY);
                while(read(fd, buffer, sizeof(buffer))>0){
                    // printf("\t\t %s", buffer);
                    printf( buffer);
                }
                close(fd);
            }
            else
                printf("%s: not a file\n", path);
        }
    }
}
int mkdir_(commandArgs command, char *cursor, streams streams){
    char path[STR_BUFOR_SIZE];
    char buffer[256];
    for(int i = 1; i < command.argc; i++){
        strcpy(path, cursor);
        move_path(path, command.argv[i]);
        if(check_path(path) < 0){
            mkdir(path, 0700);
        }
        else
            printf("%s already exist\n", path);
    }
}

// #endregion

// #region history
void save_entry(char *entry){
    FILE *history = fopen("history", "a");
    fprintf(history, "\n%s", entry);
    fclose(history);
}
char *load(char *out){
    char buffer[64];
    FILE *history = fopen("history", "r");
    while(fgets(buffer, sizeof(buffer), history)!=NULL){
        // printf(">%s", buffer);
        strcpy(out, buffer);
    }
    fclose(history);
    out[strlen(out)-1] = '\0';
    return out;
}
char *get_entry(char *out, int t){
    char buffer[64];
    FILE *history = fopen("history", "r");
    int n = 0;
    while(fgets(buffer, sizeof(buffer), history)!=NULL){
        // printf("\nread buffer: ");
        // printf("\nread buffer: %s\n", buffer);
        char *ptr = buffer;
        char *entrystart = NULL;
        while (*ptr != '\0')
        {
            if(*ptr == '\n'){
                // printf("\\n", buffer);
                n++;
                if(n==t){
                    entrystart = ptr+1;
                }
                if(n>t){
                    // ptr++;
                    *ptr = '\0';
                    break;
                }
            }
            // else printf("%c", *ptr);
            ptr++;
        }
        if(n<t){
            continue;
        }

        if(entrystart!=NULL){
            strcpy(out, entrystart);
        }
        else{
            strcat(out, buffer);
        }

        if(n>t){
            ptr = '\0';
            break;
        }
        
        // printf(">%s", buffer);
    }
    fclose(history);
    // out[strlen(out)-1] = '\0';
    return out;
}
int histlen(){
    char buffer[64];
    FILE *history = fopen("history", "r");
    int n = 0;
    while(fgets(buffer, sizeof(buffer), history)!=NULL){
        char *ptr = buffer;
        while (*ptr != '\0'){
            if(*ptr == '\n'){
                n++;
            }
            ptr++;
        }
    }
    return n+1;
}
// #endregion

// #region terminal_tricks
void canon(struct termios *old){
    if (tcgetattr(0, old) < 0)
            perror("tcsetattr()");
    old->c_lflag &= ~ICANON;
    old->c_lflag &= ~ECHO;
    old->c_cc[VMIN] = 1;
    old->c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, old) < 0)
            perror("tcsetattr ICANON");
}
void uncanon(struct termios *old){
    old->c_lflag |= ICANON;
    old->c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, old) < 0)
            perror ("tcsetattr ~ICANON");
}

void show_prompt(char *cursor){
    char pathname[STR_BUFOR_SIZE];
    char *home = getenv("HOME");
    int n = strlen(home);
    if(!strncmp(home, cursor, n)){
        // if (cursor[n]=='\0')
        //     strcpy(pathname, "~");
        // else
        //     strcpy(pathname, "~/");
        strcpy(pathname, "~");  
        strcat(pathname, cursor + n);
    }
    else{
        strcpy(pathname, cursor);
    }
    char hostname[256];
    if(gethostname(hostname, sizeof(hostname))==-1) {strcpy(hostname, "(null)");}
    printf(BOLD);
    printf(CYAN);
    printf("[%s@%s", getenv("USER"), hostname);
    printf(WHITE);
    printf(" %s", pathname);
    printf(CYAN);
    printf("]$ ");
    printf(NOCOLOR);
    printf(WHITE);
}
// #endregion

// #region old_input
// void parse_input(char *input, char *cursor){
//     save(input);
//     printf("\n");
//
//         // if(!strncmp(input,"load",4))   {  
//         //     char buffer[64]; 
//         //     load(buffer); 
//         //     clearerr(stdin); 
//         //     fprintf(stdin, buffer); 
//         //     fgets(input, sizeof(input), stdin);
//         //     input[strlen(input)-1] = '\0'; // usuń enter
//         // }
//     char c;
//     char *command = NULL;
//     int argc = 0;
//     char **args = calloc(10, sizeof(char));
//     char *token = NULL;
//     int in_quotes = 0;
//     int borrowed_time = 0;
//     for (int i = 0; (c = input[i]) != '\0'; i++){
//         if(token==NULL){
//             token = input+i;
//         }
//         if(!in_quotes){
//             if(c=='|'){// run command here
//                 token = NULL;
//                 command = NULL;
//                 argc = 0;
//             }
//             else if(c==' '){
//                 if(command==NULL){
//                     command = token;
//                 }
//                 else{
//                     args[argc] = token;
//                     argc++;
//                 }
//                 token = NULL;
//                 input[i-borrowed_time] = '\0'; //how to use borrowed time?
//             }
//         }
//         if(c=='"'){
//             in_quotes = !in_quotes;
//             borrowed_time++;
//         }
//     }
//
//     if(command==NULL){
//         command = token;
//     }
//     else{
//         args[argc] = token;
//         argc++;
//     }
//     token = NULL;
//     // run command
//     command = NULL;
//     argc = 0;
//
//     free(args);
//  
//     // char *token = strtok(input, "|");
//     // FILE *in = stdin;
//     // FILE *out = stdout;
//     // int i = 0;
//     // int pipes[10][2];
//
//     // printf("> in: %d, out: %d, err %d\n", stdin, stdout, stderr);
//     // while(token != NULL){
//     //     char *next_token = strtok(NULL, "|");
//     //     if(next_token != NULL ){
//     //         if ( pipe(pipes[i]) ){
//     //             fprintf (stderr, "Pipe failed.\n");
//     //             return EXIT_FAILURE;
//     //         }
//     //         close(pipes[i][0]);
//     //         out = fdopen(pipes[i][0], "r");
//     //         command(token, cursor, in, out, stderr);
//     //         close(pipes[i][1]);
//     //         in = fdopen(pipes[i][1], "w");
//     //     }
//     //     else{
//     //         command(token, cursor, in, stdout, stderr);
//     //     }
//     //     token = next_token;
//     //     i++;
//     // }
//     // input[0] = '\0';
// }
// #endregion

// #region handle_commands
void parse_command(commandArgs *command, char *input){
    char *token = strtok(input, " ");
    command->name = token;
    command->argc = 0;
    int redirect_n = 0;
    while( token != NULL ){
        int redirect_prefix_size = 0;
        if(!strncmp(token, ">>", 2) || !strncmp(token, "1>", 2) || !strncmp(token, "2>", 2)){
            redirect_prefix_size = 2;
        }
        else if(!strncmp(token, ">", 1) || !strncmp(token, "<", 1)){
            redirect_prefix_size = 1;
        }
        if(redirect_prefix_size != 0){
            if(token[redirect_prefix_size]=='\0'){
                char *next_token = strtok(NULL, " "); // what if NULL?
                for(char *idx = token+redirect_prefix_size; idx<next_token; idx++){
                    *idx = ' ';
                }
            }
            command->redirects[redirect_n] = token;
            redirect_n++;
            token = strtok(NULL, " ");
            continue;
        }
        
        command->argv[command->argc] = token;
        command->argc++;
        token = strtok(NULL, " ");
    }
    if (command->name==NULL){
        command->name = "";
    }
    command->argv[command->argc] = NULL;
    command->redirects[redirect_n] = NULL;
}
int get_commands(commandArgs *commands, char *input){
    int i = 1;
    // char *token = strtok(input, "|");
    char *token = strchr(input, '|');
    commandArgs *command = commands;
    while( token != NULL ){
        *token = '\0';
        token++;
        // printf("token(%s)\n", input);
        parse_command(command, input);
        input = token;
        token = strchr(input, '|');
        command++;
        i++;
    }
    parse_command(command, input);
    command++;
    command->name=NULL;
    return i;
}
int run_command(commandArgs command, char* cursor, streams streams){
    if(!strcmp(command.name,"exit"))   { return 1; }
    else if(!strcmp(command.name,"cd"))     { cd(command, cursor); return 0;}

    // pid_t id = fork();
    // if(id==0){
        if(!strcmp(command.name,"ls"))     { ls(command, cursor); }
        else if(!strcmp(command.name,"echo"))   { echo(command, cursor); }
        else if(!strcmp(command.name,"cat"))    { cat(command, cursor); }
        else if(!strcmp(command.name,"mkdir"))    { mkdir_(command, cursor, streams); }
        else if(!strcmp(command.name,"help"))    { help(command, cursor); }
        else{
            printf("looking in PATH\n");
            char *paths = getenv("PATH");
            char *path = strtok(paths,":");
            char bin_path[STR_BUFOR_SIZE];
            
            int found_command=0;
            while (path != NULL)
            {
                strcpy(bin_path, path);
                strcat(bin_path, "/");
                strcat(bin_path, command.name);
                // printf("PATH = %s: ", bin_path);
                if(execv(bin_path, command.argv) != -1){
                    found_command = 1;
                    // printf("%d\n", 1);
                    break;
                }
                // printf("%s\n", strerror(errno));
                path = strtok(NULL,":");
            }
            // printf("PATH = %s: ", command.name);
            // if(execvp(command.name, command.argv) != -1){
            //     found_command = 1;
            //     printf("%d\n", 1);
            // }
            // else 
            //     printf("%s\n", strerror(errno));
            
            if(!found_command){
                printf("nie znaleziono polecenia: {%s}\n", command.name);
                // printf("args(%d):\n", command.argc);
                // for(int i = 0; i < command.argc; i++){
                //     printf("> %s\n", command.argv[i]);
                // }
            }
        }
    //     exit(0);
    // }
    // else{
    //     wait(NULL);
    // }
    // fclose(streams.in);
    // fclose(streams.out);
    // fclose(streams.err);
    return 0;
}
int handle_input(typingField *field, char* cursor, streams streams0){
    printf("\n");
    char comstr[STR_BUFOR_SIZE];
    strcpy(comstr, field->start);
    commandArgs commands[ARG_BUFOR_SIZE]; 
    
    int commands_n = get_commands(commands, comstr);
    clear_field(field);
    
    int exit_program = 0;
    // streams redirected = streams0;
    int pipefd[2] = {-1, -1};
    int fdin = -1;
    int fdout = -1;
    int pids[ARG_BUFOR_SIZE];
    int proces_n = 0;

    int run_in_main_process = 0;
    
    if((commands+1)->name == NULL){
        if(!strcmp(commands->name,"cd"))     { 
            cd(*commands, cursor); 
            run_in_main_process = 1;
        }
    }
    
    streams *redirects = calloc(commands_n, sizeof(streams));

    for(int i = 0; i < commands_n; i++){ // redirects + init
        redirects[i].in = STDIN_FILENO;
        redirects[i].out = STDOUT_FILENO;
        redirects[i].err = STDERR_FILENO;

    }
    for(int i = 0; i < commands_n - 1; i++){ //piping

        if (pipe(pipefd) == -1) {   // pipe making  https://lms.amu.edu.pl/sci/mod/page/view.php?id=27019
            perror("pipe error");
            exit(EXIT_FAILURE);
        }
        // if(redirects[i].in == 0) redirects[i+1].in = pipefd[0];
        // if(redirects[i].out == 1) redirects[i].out = pipefd[1];
        redirects[i+1].in = pipefd[0];
        redirects[i].out = pipefd[1];
    }
    printf("n = %d\n", commands_n);
    if(!run_in_main_process) {
        for(int i = 0; i < commands_n; i++){
            commandArgs *command = commands + i;
            pid_t id = fork();
            if(id==0){
                for(int j = 0; j < commands_n; j++){
                    if(j != i){
                        if (redirects[j].in != 0)  close(redirects[j].in);
                        if (redirects[j].out != 1) close(redirects[j].out);
                        if (redirects[j].err != 2) close(redirects[j].err);
                    }
                }

                char pathin[STR_BUFOR_SIZE] = "";
                char pathout[STR_BUFOR_SIZE] = "";
                char patherr[STR_BUFOR_SIZE] = "";
                for(char **redirect_ptr = command->redirects; redirect_ptr[0] != NULL; redirect_ptr++){
                    char *redirect = *redirect_ptr;
                    char *paths[2] = {NULL,NULL};  
                    if(!strncmp(redirect, "&>",2)){
                        paths[0] = pathout;
                        paths[1] = patherr;
                        redirect++;
                    }
                    else if(!strncmp(redirect, "2>",2)){
                        paths[0] = patherr;
                        redirect++;
                    }
                    else if(!strncmp(redirect, "1>",2)){
                        paths[0] = pathout;
                        redirect++;
                    }
                    else if(!strncmp(redirect, ">",1)){
                        paths[0] = pathout;
                    }
                    else if(!strncmp(redirect, "<",1)){
                        paths[0] = pathin;
                    }
                    for(int i = 0; paths[i] != NULL; i++){
                        char *path = paths[i];
                        while( *(++redirect) == ' ' );
                        printf("redirect: %s\n", redirect);
                        strcpy(path, cursor);
                        move_path(path, redirect);
                    }
                }
                if(pathin[0] != '\0'){
                    if(redirects[i].in != 0)
                        close(redirects[i].in);
                    redirects[i].in = open(pathin, O_RDONLY, 0644);
                }
                if(pathout[0] != '\0'){
                    if(redirects[i].out != 1)
                        close(redirects[i].out);
                    redirects[i].out = open(pathout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }
                if(patherr[0] != '\0'){
                    if(redirects[i].err != 2)
                        close(redirects[i].err);
                    redirects[i].err = open(patherr, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }


                // close(pipefd[1]);
                printf("hi from %d\n", getpid());
                dup2(redirects[i].in, STDIN_FILENO);
                dup2(redirects[i].out, STDOUT_FILENO);
                dup2(redirects[i].err, STDERR_FILENO);
                int code = run_command(*command, cursor, redirects[i]);
                fflush(stdout);
                printf("zamykamy proces %d z komendą [%s]\n", getpid(), command->name);
                // close(pipefd[0]); 
                // if( out != NULL) fclose(out);
                // else if (redirected.out != 1) close(redirected.out);
                if (redirects[i].in != 0)  close(redirects[i].in);
                if (redirects[i].out != 1) close(redirects[i].out);
                if (redirects[i].err != 2) close(redirects[i].err);
                exit(code);
            }
            else{
                printf("rozpoczęto proces %d z komendą [%s]\n", id, command->name);
                // close(pipefd[1]); 
                // if (redirected.out != 1) close(redirected.out);
                pids[proces_n] = id;
                proces_n += 1;
            }
        }
        for(int j = 0; j < commands_n; j++){
            // close(redirects[j].in);
            // close(redirects[j].out);
            // close(redirects[j].err);
            if (redirects[j].in != 0)  close(redirects[j].in);
            if (redirects[j].out != 1) close(redirects[j].out);
            if (redirects[j].err != 2) close(redirects[j].err);
        }
        printf("!\n");

        // if(pipefd[0] != -1){ // close pipe
        //     close(pipefd[0]); 
        //     close(pipefd[1]); 
        // }
        int j = proces_n;
        while (j > 0)
        {
            
            int status;
            int id = wait(&status);
            for(int i = 0; i < proces_n; i++){
                if(id == pids[i]){
                    pids[i] = 0;
                }
            }
            if (WIFEXITED(status)){
                exit_program = WEXITSTATUS(status);
                if(exit_program){
                    return 1;
                }
            }
            j--;
        }
    }
    // return run_command(command, cursor, streams);

    return exit_program;
}

int input_char(typingField *field, int hlen){
    char c = getchar();
    if(c=='\e'){
        if((c=getchar()) == '['){ // wciśnięto strzałke
            c = getchar();
            if(c=='D'){ // LEFT
                move_cursor(-1, field);
            }
            else if(c=='C'){ // RIGHT
                move_cursor(1, field);
            }
            else if(c=='A'){ // UP
                field->backtrack++;
                // field->backtrack = field->backtrack<hlen ? field->backtrack : hlen;
                if(hlen - field->backtrack >= 0){
                    clear_field(field);
                    char s[STR_BUFOR_SIZE];
                    get_entry(s, hlen - field->backtrack);
                    types(s, field);
                }
                else field->backtrack--;
            } 
            else if (c=='B'){ // DOWN
                field->backtrack--;
                if(field->backtrack > 0){
                    clear_field(field);
                    char s[STR_BUFOR_SIZE];
                    get_entry(s, hlen - field->backtrack);
                    types(s, field);
                }
                else if(field->backtrack == 0){
                    clear_field(field);
                }
                else field->backtrack++;
            }
            else if(c=='1' && getchar()==';' && getchar()=='5'){ // trzymany ctrl
                c = getchar();
                if(c=='D'){ // ^LEFT
                    set_cursor(token_left(field->cursor, field), field);
                }
                else if(c=='C'){ // ^RIGHT
                    set_cursor(token_right(field->cursor, field), field);
                }
                else{printf("[[[%c]]]",c);}
            }
            else if (c=='3' && getchar()=='~'){ // delete
                deletec(field); 
            }
            else if (c=='H'){ // HOME
                set_cursor(field->start, field); 
            }
            else if (c=='F'){ // END
                set_cursor(field->end, field); 
            }
            else { printf("[[%c]]",c); }
        }
        else if (c=='d'){ // CTRL+DELETE
            tail_to_cursor(token_right(field->cursor, field)+1, field);
        }
        else if (c=='\e'){ printf("\n"); return 1; }
        else { printf("[%c]",c); }
    }
    else if (c<=26){ //operacje z ctrl
        char d = c+64;
        if(d=='W'){ //backspace
            int delta = set_cursor(token_left(field->cursor, field), field);
            tail_to_cursor(field->cursor - delta, field);
        }
        else if (d=='J'){//ENTER
            // printf("\n[%s]", field->start);
            // return run_command(field);
            return 1;
        } 
        else if (d=='D'){// INTERUPT
            return 1;
        }
        else{
            printf("^%c", d);
        }
    }
    else if (c==127){ //backspace
        backspace(field);
    }
    else{
        typec(c, field);
    }
    return 0;
}
// #endregion

int main() {
    int hlen = histlen();
    long size = pathconf(".", _PC_PATH_MAX); // https://pubs.opengroup.org/onlinepubs/007904975/functions/getcwd.html
    char *cursor = (char *)malloc((size_t)size);
    // czemu potrzebny osobny bufer???
    // char *cursor;
    // char *buffer = (char *)malloc((size_t)size); 
    // if ((buffer != NULL)){
    //     cursor = getcwd(buffer, (size_t)size);
    if ((cursor != NULL)){ // działa bez niego wtf?
        getcwd(cursor, (size_t)size);
    }
    else
        printf("jakiś błąd");
    
    typingField field;
    char input_bufor[STR_BUFOR_SIZE] = "";
    // *field.input = "";
    field.start = input_bufor;
    field.cursor = input_bufor;
    field.end = input_bufor;
    field.backtrack = 0;



    struct termios old = {0};
    canon(&old);
    streams streams;
    // streams.in  = stdin;
    // streams.out = stdout;
    // streams.err = stderr;
    streams.in  = STDIN_FILENO;
    streams.out = STDOUT_FILENO;
    streams.err = STDERR_FILENO;
    show_prompt(cursor);
    while(1){
        fflush(stdout);
        fflush(stdin);
        // int len = strlen(input);
        // int len = field.len;
        if (input_char(&field, hlen)) {
            save_entry(field.start);
            uncanon(&old);
            if (handle_input(&field, cursor, streams)){
                break;
            }
            hlen++;
            canon(&old);
            show_prompt(cursor);
        };
    }
    uncanon(&old);
    free(cursor);
    printf("koniec\n");
    return 0;
}
