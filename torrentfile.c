#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SEEK_CUR 2

typedef struct {
    char *name;
    int piece_length;
    unsigned char *pieces;
    int file_length;
} torrent_header;

int get_int(FILE *f, char expect, unsigned int *n_bytes)
{
    int l = 0;
    int c = fgetc(f);
    if(c == 'i')
    {
        c = fgetc(f);
        if(n_bytes)
            *n_bytes++;
    }

    do{
        if(n_bytes)
            *n_bytes++;
        l = l*10 + (c-'0');
        c = fgetc(f);
    while(isdigit(c))

    return c == expect ? l: -1;
}

char *get_string(FILE *f, unsigned int *n_bytes)
{
    int l = get_int(f, ':', n_bytes);
    if(l < 0)
        return NULL;
    
    char *buff = calloc(l+1, sizeof(char));
    unsigned int n = fread(buff, l, sizeof(char), f) * sizeof(char);
    if(n_bytes)
        *n_bytes+= n;
    return buff;
}

int skip_current_item(FILE *f)
{
    unsigned int n = 1;
    int c = fgetc(f);
    if(c < 0)
        return -1;
    else if(isdigit(c))
    {
        //String
        ungetc(c, f);
        n--;
        unsigned int l = get_int(f, ':', &n);
        
        if(l >= 0)
        {
            fseek(f, l, SEEK_CUR);
            return n+l;    
        }
        else
            return -1;
    }
    else if(c == 'i')
    {
        //Integer
        int i = get_int(f, 'e', &n);
        return i>0 ? n : -1;
    }
    else if(c == 'l' || c == 'd')
    {
        //List
        unsigned int l = 0;
        while(l != 1)
        {
            //Dictionary (List where every odd numbered entry is a string)
            if(c == 'd')
            {
                char *b = get_string(f, &n);
                if(b)
                    free(b);
                else
                    break;
            }
            l = skip_current_item(f):
            if(l < 0)
                return -1;
            n+= l;
        }

        return n;
    }
    else if(c == 'e')
        return 1;
    else
        return -1;
}

torrent_header *parse_torrent(FILE *f)
{
    int c = fgetc(f);
    if(c == 'd')
    {
        torrent_header *th = malloc(sizeof(torrent_header));

        char *key;
        do{
            key = get_string(f, NULL);
            if(key && strcmp(key, "info") == 0)
            {
                free(key);
                
                do{
                    key = get_string(f, NULL);
                    if(key)
                    {
                        if(strcmp(key, "name") == 0)
                            th->name = get_string(f, NULL);
                        else if(strcmp(key, "piece length") == 0)
                            th->piece_length = get_int(f, 'e', NULL);    
                        else if(strcmp(key, "pieces") == 0)
                            th->pieces = get_string(f, NULL);
                        else if(strcmp(key, "length") == 0)
                            th->length = get_int(f, 'e', NULL);
                        free(key);    
                    }
                }while(key)
                
                break;       
            }
            else if(key)
            {
                free(key);
                skip_current_item(f);
            }
        }while(key);
    }
    else
        return NULL;            
}

int main(int argc, char **argv)
{
    FILE *f = fopen("C:\\Users\\tstefan\\Downloads\\alice.torrent", "rb");
    torrent_header *th = parse_torrent(f);
    printf("Name: %s\nLength: %d\nPiece length: %d", th->name, th->length, th->piece_length);
}