#include <stdio.h>
#include <dirent.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

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
}commandArgs;

typedef struct streams{
    FILE *__restrict__ in;
    FILE *__restrict__ out;
    FILE *__restrict__ err;
}streams;


#ifndef typing_fields
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
#endif

#ifndef paths
typedef struct path_cursor{
    char *path;
    char *user;
}path_cursor;
int check_in_path(char *path, char *name){
    struct dirent *entry; // https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    DIR *dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (!strcmp(entry->d_name, name)){
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);
    }
    return 0;
}
char *take_path_token(char **path){ // consumes path
    // printf("\ttaking token from [%s] [%d]\n", *path, *path[0] == '/');
    if (*path[0] == '/'){
        *path = *path + 1;
        // printf("\ttaking token from [%s] [%d]\n", *path, *path[0] == '/');
        return "/";
    }
    else{
        char *new_path = strchr(*path, '/');
        if(new_path == NULL){
            char *out = *path;
            *path = strchr(*path, '\0');
            return out;
        }
        else{
            char *out = *path;
            new_path[0] = '\0';
            *path = new_path + 1;
            return out; 
        }
    }
}
int read_path(char *from, char *path){
    while (path[0] != '\0')
    {
        // printf("path [%s]\n", path);
        char *token = take_path_token(&path);

        // printf("token [%s]\t\tpath [%s]\n\n", token, path);

        if(!strcmp(token, "/")){
            strcpy(from, token);
            return read_path(from, path);
        }
        else if(!strcmp(token, ".")){
            return read_path(from, path);
        }
        else if(!strcmp(token, "..")){
            char *slash = strrchr(from, '/');
            if (slash == from)
                slash[1] = '\0';
            else
                slash[0] = '\0';
            return read_path(from, path);
        }
        else{
            if( !check_in_path(from, token) ){
                return 1;
            }
            else{
                char *to_move_end = strchr(from, '\0');
                if (to_move_end[-1] != '/')
                    strcat(from, "/");
                strcat(from, token);
                return read_path(from, path);
            }
        }
    }
    return 0;
}
#endif

#ifndef built_ins
void ls(commandArgs command, char *cursor, streams streams){
    DIR *dir;
    struct dirent *entry; // https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    dir = opendir(cursor);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            fprintf(streams.out, "%s\n", entry->d_name);
        }
        closedir(dir);
    }
}
int cd(commandArgs command, char *cursor, streams streams){
    if(command.argc<=1) return 1;
    char *path = command.argv[1];

    char new_cursor[STR_BUFOR_SIZE];
    if (path[0] != '/'){
        strcpy(new_cursor, cursor);
    }
    if (!read_path(new_cursor, path)){
        strcpy(cursor, new_cursor);
        return 0;
    }
    else{
        fprintf(streams.err, "cd: path not found [%s] -> [%s]\n", new_cursor, path);
        return 1;
    }
}
int echo(commandArgs command, char *cursor, streams streams){
    for(int i = 1; i<command.argc; i++){
        char *message = command.argv[i];
        // printf("\t\t> echo: '%s' to %d\n", message, streams.out);
        fprintf(streams.out, "%s\n", message);
    }
    return 0;
}
int cat(commandArgs command, char *cursor, streams streams){
    // printf("\t\t> cat from %d to %d: \n", in, out);
    if(command.argc <= 1){
        char buffer[256];
        while(fgets(buffer, sizeof(buffer), streams.in) != NULL){
            // printf("\t\t %s", buffer);
            fprintf(streams.out, buffer);
        }
        return 0;
    }
    else{
        fprintf(streams.out, "TODO: cat files");
    }
}
#endif

#ifndef history
// typedef struct history{
//     char *entries[STR_BUFOR_SIZE];
//     int count;
//     int current;
// }history;
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
#endif

#ifndef terminal_tricks
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

void show_prompt(path_cursor cursor){
    char hostname[256];
    if(gethostname(hostname, sizeof(hostname))==-1) {strcpy(hostname, "(null)");}
    printf(BOLD);
    printf(CYAN);
    printf("[%s@%s", cursor.user, hostname);
    printf(WHITE);
    printf(" %s", cursor.path);
    printf(CYAN);
    printf("]$ ");
    printf(NOCOLOR);
    printf(WHITE);
}
#endif

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

#ifndef handle_commands
void get_command(commandArgs *command, char *bufor, typingField *field){
    strcpy(bufor, field->start);
    clear_field(field);
    char *token = strtok(bufor, " ");
    command->name = token;
    command->argc = 0;
    while( token != NULL ){
        command->argv[command->argc] = token;
        command->argc++;
        token = strtok(NULL, " ");
    }
    if (command->name==NULL){
        command->name = "";
    }
}
int run_command(typingField *field, path_cursor cursor, streams streams){
    printf("\n");
    char comstr[STR_BUFOR_SIZE];
    commandArgs command;
    get_command(&command, comstr, field);

    if(!strncmp(command.name,"exit",4))   { return 1; }
    else if(!strncmp(command.name,"cd",2))     { cd(command, cursor.path, streams); return 0;}

    pid_t id = fork();
    if(id==0){
        if(!strncmp(command.name,"ls",2))     { ls(command, cursor.path, streams); }
        else if(!strncmp(command.name,"echo",4))   { echo(command, cursor.path, streams); }
        else if(!strncmp(command.name,"cat",3))    { cat(command, cursor.path, streams); }
        else{
            char *paths = getenv("PATH");
            char *path = strtok(paths,":");
            char bin_path[STR_BUFOR_SIZE];
            
            int found_command=0;
            while (path != NULL)
            {
                strcpy(bin_path, path);
                strcat(bin_path, "/");
                strcat(bin_path, command.name);
                if(execv(bin_path, command.argv) != -1){
                    found_command = 1;
                    break;
                }
                printf("PATH = %s: %d\n", bin_path, found_command);
                path = strtok(NULL,":");
            }
            
            if(!found_command){
                printf("nie znaleziono polecenia: {%s}\n", command.name);
                // printf("args(%d):\n", command.argc);
                // for(int i = 0; i < command.argc; i++){
                //     printf("> %s\n", command.argv[i]);
                // }
            }
        }
        exit(0);
    }
    else{
        wait(NULL);
    }

    return 0;
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
#endif

int main() {
    int hlen = histlen();

    path_cursor cursor;
    cursor.user = getenv("USER");
    printf(getenv("HOME"));
    long size = pathconf(".", _PC_PATH_MAX); // https://pubs.opengroup.org/onlinepubs/007904975/functions/getcwd.html
    cursor.path = (char *)malloc((size_t)size);
    // czemu potrzebny osobny bufer???
    // char *cursor;
    // char *buffer = (char *)malloc((size_t)size); 
    // if ((buffer != NULL)){
    //     cursor = getcwd(buffer, (size_t)size);
    if ((cursor.path != NULL)){ // działa bez niego wtf?
        getcwd(cursor.path, (size_t)size);
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
    streams.in = stdin;
    streams.out = stdout;
    streams.err = stderr;
    show_prompt(cursor);
    while(1){
        fflush(stdout);
        fflush(stdin);
        // int len = strlen(input);
        // int len = field.len;
        if (input_char(&field, hlen)) {
            save_entry(field.start);
            uncanon(&old);
            if (run_command(&field, cursor, streams)){
                break;
            }
            hlen++;
            canon(&old);
            show_prompt(cursor);
        };
    }
    uncanon(&old);
    free(cursor.path);
    printf("koniec\n");
    return 0;
}
