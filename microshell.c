#include <stdio.h>
#include <dirent.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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



    char input[256];
    char hostname[256];
    while(1){
        if(gethostname(hostname, sizeof(hostname))==-1) {strcpy(hostname, "(null)");}
        printf("[%s@%s %s]$ ", getenv("USER"), hostname, cursor);
        // printf("< eigen | %s > ", cursor);
        fgets(input, sizeof(input), stdin);
        input[strlen(input)-1] = '\0'; // usuń enter
        if(!strncmp(input,"load",4))   {  
            char buffer[64]; 
            load(buffer); 
            clearerr(stdin); 
            fprintf(stdin, buffer); 
            fgets(input, sizeof(input), stdin);
            input[strlen(input)-1] = '\0'; // usuń enter
        }
        save(input);

        char *token = strtok(input, "|");
        FILE *in = stdin;
        FILE *out = stdout;
        int i = 0;
        int pipes[10][2];

        printf("> in: %d, out: %d, err %d\n", stdin, stdout, stderr);
        while(token != NULL){
            char *next_token = strtok(NULL, "|");
            if(next_token != NULL ){
                if (pipe (pipes[i])){
                    fprintf (stderr, "Pipe failed.\n");
                    return EXIT_FAILURE;
                }
                
                close(pipes[i][0]);
                out = fdopen(pipes[i][0], "r");
                command(token, cursor, in, out, stderr);
                close(pipes[i][1]);
                in = fdopen(pipes[i][1], "w");
            }
            else{
                command(token, cursor, in, stdout, stderr);
            }
            token = next_token;
            i++;
        }
        // i--;
        // for(int j = i; i>=0; i--){
        //     // close(pipes[i][0]);
        //     close(pipes[i][1]);
        // }

        // command(input, cursor, stdin, stdout, stderr);
    }
    
    return 0;

}
