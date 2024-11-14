#include <stdio.h>
#include <dirent.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

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
    printf("[%s@%s %s]$ ", getenv("USER"), hostname, cursor);
}
void move_cursor(int amount, int len, int backspace){
    char *message = (amount>0) ? "\e[C" : "\b";
    for (int i = 0; i < abs(amount); i++)
    {
        printf(message);
    }
}
// void reconnect_tail(int from, char *input, int len, int backspace){
//     for(int i = 0; i<=len-from; i++){
//         int cur = len - backspace + i; 
//         input[cur] = input[from + i];
//         printf("%c",input[cur]);
//     }
//     for (int i = 0; i < from - (len-backspace); i++)
//     {
//         printf(" ");
//     }
//     for (int i = 0; i < len-backspace; i++)
//     {
//         printf("\b");
//     }
// }
int delete_back(char *input, int *len, int backspace){
    if(backspace >= (*len)){
        return 1;
    }
    printf("\b");
    for(int i = (*len) - backspace-1; i<(*len); i++){
        input[i] = input[i+1];
        printf("%c",input[i]);
    }
    printf(" ");
    for (int i = 0; i < backspace+1; i++)
    {
        printf("\b");
    }
    (*len)--;
    return 0;
}
int delete_front(char *input, int *len, int *backspace){
    if(backspace <= 0){
        return 1;
    }
    move_cursor(1, *len, backspace);
    (*backspace)--;
    delete_back(input, len, backspace);
}

void typec(char c, char *input, int len, int backspace){
    char ci;
    for(int i = len - backspace; i<=len; i++){
        ci = input[i];
        input[i]=c;
        printf("%c", c);
        c=ci;
    }
    input[len+1] = '\0';
    move_cursor(-backspace, len, backspace);

}
void debugc(char c, char *input, int len, int backspace){
    char ci;
    for(int i = len - backspace; i<=len; i++){
        ci = input[i];
        input[i]=c;
        printf("%d", c);
        c=ci;
    }
    input[len+1] = '\0';
    for (int i = 0; i < backspace; i++)
    {
        printf("\b");
    }
}
void parse_input(char *input, char *cursor){
    save(input);
    printf("\n");

        // if(!strncmp(input,"load",4))   {  
        //     char buffer[64]; 
        //     load(buffer); 
        //     clearerr(stdin); 
        //     fprintf(stdin, buffer); 
        //     fgets(input, sizeof(input), stdin);
        //     input[strlen(input)-1] = '\0'; // usuń enter
        // }
    char c;
    char *command = NULL;
    int argc = 0;
    char **args = calloc(10, sizeof(char));
    char *token = NULL;
    int in_quotes = 0;
    int borrowed_time = 0;
    for (int i = 0; (c = input[i]) != '\0'; i++){
        if(token==NULL){
            token = input+i;
        }
        if(!in_quotes){
            if(c=='|'){// run command here
                token = NULL;
                command = NULL;
                argc = 0;
            }
            else if(c==' '){
                if(command==NULL){
                    command = token;
                }
                else{
                    args[argc] = token;
                    argc++;
                }
                token = NULL;
                input[i-borrowed_time] = '\0'; //how to use borrowed time?
            }
        }
        if(c=='"'){
            in_quotes = !in_quotes;
            borrowed_time++;
        }
    }

    if(command==NULL){
        command = token;
    }
    else{
        args[argc] = token;
        argc++;
    }
    token = NULL;
    // run command
    command = NULL;
    argc = 0;

    free(args);
    

    // char *token = strtok(input, "|");
    // FILE *in = stdin;
    // FILE *out = stdout;
    // int i = 0;
    // int pipes[10][2];

    // printf("> in: %d, out: %d, err %d\n", stdin, stdout, stderr);
    // while(token != NULL){
    //     char *next_token = strtok(NULL, "|");
    //     if(next_token != NULL ){
    //         if ( pipe(pipes[i]) ){
    //             fprintf (stderr, "Pipe failed.\n");
    //             return EXIT_FAILURE;
    //         }
    //         close(pipes[i][0]);
    //         out = fdopen(pipes[i][0], "r");
    //         command(token, cursor, in, out, stderr);
    //         close(pipes[i][1]);
    //         in = fdopen(pipes[i][1], "w");
    //     }
    //     else{
    //         command(token, cursor, in, stdout, stderr);
    //     }
    //     token = next_token;
    //     i++;
    // }
    // input[0] = '\0';
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

    char input[256] = "";
    int backspace = 0;
    show_prompt(cursor);
    char c;

    struct termios old = {0};
    canon(&old);
    while(1){
        fflush(stdout);
        fflush(stdin);
        c = getchar();
        int len = strlen(input);
        if(c=='\e'){
            if((c=getchar()) == '['){ // wciśnięto strzałke
                c = getchar();
                if(c=='D' && backspace<len){ // LEFT
                    printf("\b");
                    backspace+=1;
                }
                else if(c=='C' && backspace>0){ // RIGHT
                    printf("\e[C");
                    backspace-=1;
                }
                else if(c=='A' || c=='B'){ // UP||DOWN
                    printf("↓↑");
                }
                else if(c=='1' && getchar()==';' && getchar()=='5'){ // trzymany ctrl
                    c = getchar();
                    int i = len - backspace;
                    if(c=='D' && i>0){ // LEFT
                        do{
                            i--;
                            printf("\b");
                            backspace+=1;
                        }while (i>0 && (input[i-1]!=' ' || input[i]==' '));
                    }
                    else if(c=='C' && i<len){ // RIGHT
                        do{
                            i++;
                            printf("\e[C");
                            backspace-=1;
                        }while (i<len && (input[i-1]!=' ' || input[i]==' '));
                    }
                    else{printf("[[[%c]]]",c);}
                }
                else { printf("[[%c]]",c); }
            }
            else if (c=='\e'){ printf("\n"); break; }
            else { printf("[%c]",c); }
            continue;
        }
        else if (c<=26){ //operacje z ctrl
            char d = c+64;
            if(d=='W'){ //backspace
                while (/* condition */)
                {
                    /* code */
                }
                
                delete_back(input, &len, backspace);
            }
            else{
                printf("^%c", d);
            }
        }
        else if (c==127){ //backspace
            delete_back(input, &len, backspace);
            // printf("\b");
            // for(int i = len - backspace-1; i<len; i++){
            //     input[i] = input[i+1];
            //     printf("%c",input[i]);
            // }
            // printf(" ");
            // for (int i = 0; i < backspace+1; i++)
            // {
            //     printf("\b");
            // }
        }
        else if (c=='\n'){
            parse_input(input, cursor);
            show_prompt(cursor);
        }
        else{
            typec(c, input, len, backspace);
        }
    }

    uncanon(&old);
    free(cursor);
    return 0;
}
