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

// const int STR_BUFOR_SIZE = 256;
// const int ARG_BUFOR_SIZE = 64;

#define STR_BUFOR_SIZE 256
#define ARG_BUFOR_SIZE 64

typedef struct typingField{
    char *start;
    char *cursor;
    char *end;
}typingField;

typedef struct commandArgs{
    char *name;
    char *args[ARG_BUFOR_SIZE];
    int argc;
}commandArgs;

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
    *field->start = '\0';
    field->cursor = field->start;
    field->end = field->start;
}

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
void ls(char *path, FILE* out){
    DIR *dir;
    struct dirent *entry; // https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            fprintf(out, "%s\n", entry->d_name);
        }
        closedir(dir);
    }
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
int move_path(char *to_move, char *path){
    while (path[0] != '\0')
    {
        // printf("path [%s]\n", path);
        char *token = take_path_token(&path);

        // printf("token [%s]\t\tpath [%s]\n\n", token, path);

        if(!strcmp(token, "/")){
            strcpy(to_move, token);
            return move_path(to_move, path);
        }
        else if(!strcmp(token, ".")){
            return move_path(to_move, path);
        }
        else if(!strcmp(token, "..")){
            char *slash = strrchr(to_move, '/');
            if (slash == to_move)
                slash[1] = '\0';
            else
                slash[0] = '\0';
            return move_path(to_move, path);
        }
        else{
            if( !check_in_path(to_move, token) ){
                return 1;
            }
            else{
                char *to_move_end = strchr(to_move, '\0');
                if (to_move_end[-1] != '/')
                    strcat(to_move, "/");
                strcat(to_move, token);
                return move_path(to_move, path);
            }
        }
    }
    return 0;
}
int cd(char *cursor, char *path, FILE* err){
    char new_cursor[250];
    if (path[0] != '/'){
        strcpy(new_cursor, cursor);
    }
    if (!move_path(new_cursor, path)){
        strcpy(cursor, new_cursor);
        return 0;
    }
    else{
        fprintf(err, "cd: path not found [%s] -> [%s]\n", new_cursor, path);
        return 1;
    }
}

int echo(char *message, FILE *out){
    printf("\t\t> echo: '%s' to %d\n", message, out);
    fprintf(out, "%s\n", message);
    return 0;
}
int cat(FILE *in, FILE *out){
    // printf("\t\t> cat from %d to %d: \n", in, out);
    char buffer[256];
    while(fgets(buffer, sizeof(buffer), in) != NULL){
        // printf("\t\t %s", buffer);
        fprintf(out, buffer);
    }
    return 0;
}

void save(char *entry){
    FILE *history = fopen("history", "a");
    fprintf(history, "%s\n", entry);
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

int command(char *input, char *cursor, FILE *in, FILE *out, FILE *err){
    printf("\t> running command '%s' \n", input);
         if(!strncmp(input,"exit",4))   { exit(0); }
    else if(!strncmp(input,"ls",2))     { ls(cursor, out); }
    else if(!strncmp(input,"cd",2))     { cd(cursor, input+3, err); }
    else if(!strncmp(input,"echo",4))   { echo(input+4, out); }
    else if(!strncmp(input,"cat",3))    { cat(in, out); }
    else                                { printf("lol, co?!?!??\n"); }
    clearerr(stdin);
}

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
    char hostname[256];
    if(gethostname(hostname, sizeof(hostname))==-1) {strcpy(hostname, "(null)");}
    printf(BOLD);
    printf(CYAN);
    printf("[%s@%s", getenv("USER"), hostname, cursor);
    printf(WHITE);
    printf(" %s", hostname, cursor);
    printf(CYAN);
    printf("]$ ");
    printf(NOCOLOR);
}

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


void get_command(commandArgs *command, char *bufor, typingField *field){
    // char comstr[STR_BUFOR_SIZE];
    strcpy(bufor, field->start);
    clear_field(field);
    command->name = strtok(bufor, " ");
    command->argc = 0;
    char *token = strtok(NULL, " ");
    while( token != NULL ){
        command->args[command->argc] = token;
        command->argc++;
        token = strtok(NULL, " ");
    }
    // return command;
}
void run_command(typingField *field){
    char comstr[STR_BUFOR_SIZE];
    commandArgs command;
    get_command(&command, comstr, field);
    printf("\nrunning: %s\n", command.name);
    for(int i = 0; i < command.argc; i++){
        printf("> %s\n", command.args[i]);
    }
}

int input_char(typingField *field){
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
            else if(c=='A' || c=='B'){ // UP||DOWN
                printf("↓↑");
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
            run_command(field);
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


int main() {
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


    // int backspace = 0;
    show_prompt(cursor);
    char c;

    struct termios old = {0};
    canon(&old);
    while(1){
        fflush(stdout);
        fflush(stdin);
        // int len = strlen(input);
        // int len = field.len;
        if (input_char(&field)) break;
        
    }

    uncanon(&old);
    free(cursor);
    printf("\nkoniec\n");
    return 0;
}
