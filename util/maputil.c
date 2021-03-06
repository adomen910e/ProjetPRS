/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   maputil.c
 * Author: norips
 *
 * Created on 29 octobre 2016, 14:41
 */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "../include/map.h"
#include "function.h"
#define NB_FIELD_ON_LINE 6
/*
 * 
 */
static struct option long_options[] = {
    {"getwidth", no_argument, NULL, 'w'},
    {"getheight", no_argument, NULL, 'h'},
    {"getobjects", no_argument, NULL, 'o'},
    {"getinfo", no_argument, NULL, 'i'},
    {"setwidth", required_argument, NULL, 'x'},
    {"setheight", required_argument, NULL, 'y'},
    {"setobjects", required_argument, NULL, 'a'},
    {"pruneobjects", no_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

/**
 * Set new width to the file
 * @param fd_file Open file descriptor to valid save file RDWR
 * @param width
 */
void setWidth(int fd_file, char* width);

/**
 * Set new height to the file
 * @param fd_file
 * @param height
 */
void setHeight(int fd_file, char* height);

/**
 * Reset and add argc-optind numbers of objects to save file
 * @param fd_file
 * @param argv
 * @param argc
 * @param optind Starting indice of parameters position in argv
 */
void addObject(int fd_file, char **argv, int argc, int optind);

void removeUnused(int file);
void addObject(int file, char **argv, int argc, int optind);

int main(int argc, char** argv) {
    int ch;
    int file = open(argv[1], O_RDWR);
    if (file == -1) {
        fprintf(stderr, "Need a valid file\n");
        exit(EXIT_FAILURE);
    }
    while ((ch = getopt_long(argc, argv, "w:h:o:i:x:y:a:p", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch) {
            case 'w':
                printWidth(file);
                break;
            case 'h':
                printHeight(file);
                break;
            case 'o':
                printObject(file);
                break;
            case 'i':
                printInfo(file);
                break;
            case 'x':
                setWidth(file, optarg);
                break;
            case 'y':
                setHeight(file, optarg);
                break;
            case 'a':;
                unsigned int curr_nb_obj = getObject(file);
                if (curr_nb_obj > ((argc - (optind-1)) / NB_FIELD_ON_LINE)) {
                    fprintf(stderr, "Need more or equal number of objects than %d", curr_nb_obj);
                    exit(EXIT_FAILURE);
                }
                addObject(file, argv, argc, optind);
                break;
            case 'p': ;//
                removeUnused(file);
                break;
        }
    }
    close(file);
    return (EXIT_SUCCESS);
}

void removeUnused(int file) {
    //Get current number of object
    unsigned int nbObj = getObject(file);
    if(nbObj<=0) {
        fprintf(stderr,"removeUnused : nb_obj<0");
        return;
    }
    //Create an array of char to store if a type of object is present and need to be save
    char *objInUse = malloc(sizeof(char)*nbObj);
    //Fill with zero
    memset(objInUse,0,nbObj);
    lseek(file, 0, SEEK_SET);
    //Go after basic info, before first object type
    free(getLine(file));
    for(int i = 0; i < nbObj; i++)
        free(getLine(file));
    //We are after objects type,before first object
    char *tmp = getLine(file);
    int nbInUse = 0;
    while(strcmp(tmp,"END")){
        strtok(tmp,"\t");
        strtok(NULL,"\t");
        char *_objId = strtok(NULL,"\t");
        //Get object ID
        int objId = atoi(_objId);
        //If not already register to be saved
        if(!objInUse[objId]){
            objInUse[objId] = 1;
            //Count number of object to be saved
            nbInUse++;
        }
        free(tmp);
        //Next object
        tmp = getLine(file);
    }
    free(tmp);
    int tmpFile = open("/tmp/tmpFileremoveUnused", O_CREAT | O_TRUNC | O_RDWR, 0666);
    int saveOut = dup(1);
    dup2(tmpFile, 1);
    lseek(file,0,SEEK_SET);
    //Write new info to tmp file Width Height Objects (who))
    char* who = getLine(file);
    write(tmpFile,who,sizeof(unsigned int));
    write(tmpFile,who+sizeof(unsigned int),sizeof(unsigned int));
    //number of real object type in use
    write(tmpFile,&nbInUse,sizeof(unsigned int));
    free(who);
    printf("\n");

    //This will contains like this newObjID[oldid]
    int newObjID[nbObj];
    int currID = 0;
    for(int i = 0; i < nbObj;i++){
        char *tmp = getLine(file);
        //Is this object need to be saved ?
        if(objInUse[i]){
            //Yes new id is currID
            newObjID[i] = currID++;
            printf("%s\n",tmp);
            fprintf(stderr,"%d use : %s\n",i,tmp);
        }
        free(tmp);
    }
    tmp = getLine(file);
    //For each objects
    while(strcmp(tmp,"END")){
        char *tk = tmp;
        char *x = strtok(tk, "\t");
        char *y = strtok(NULL, "\t");
        char *_oldId = strtok(NULL, "\n");
        int    oldId = atoi(_oldId);

        //Write is new ID
        printf("%s\t%s\t%d\n",x,y,newObjID[oldId]);
        free(tmp);
        tmp = getLine(file);
    }
    free(tmp);
    printf("END\n");
    fflush(stdout);
    copyAndTruncate(tmpFile,file);
    free(objInUse);
    close(tmpFile);
    dup2(saveOut,1);
    
    
}

void addObject(int file, char **argv, int argc, int optind) {
    int tmpFile = open("/tmp/tmpFileObjMapUtil", O_CREAT | O_TRUNC | O_RDWR, 0666);
    int save_out = dup(1);
    dup2(tmpFile, 1);
    lseek(file, 0, SEEK_SET);
    
    unsigned int width = getWidth(file);
    unsigned int height = getHeight(file);
    unsigned int nb_obj = getObject(file);
    unsigned int new_nb_obj = (argc - optind) / NB_FIELD_ON_LINE;
    write(tmpFile,&width,sizeof(unsigned int));
    write(tmpFile,&height,sizeof(unsigned int));
    write(tmpFile,&new_nb_obj,sizeof(unsigned int));

    printf("\n");
    free(getLine(file));
    optind--;
    for(int i=0; i < nb_obj;i++){
        free(getLine(file));
    }

    for (; optind < argc && *argv[optind] != '-'; optind += NB_FIELD_ON_LINE) {//Next obj
	for(int i = 0; i < NB_FIELD_ON_LINE; i++)
        	printf("%s\t", argv[optind +i]);
        printf("\n");
    }
    char *tmp = getLine(file);
    while(strcmp(tmp,"END")){
        printf("%s\n",tmp);
        free(tmp);
        tmp = getLine(file);
    }
    printf("END\n");
    fflush(stdout);
    dup2(save_out, 1);
    copyAndTruncate(tmpFile,file);
    close(tmpFile);


}

void setWidth(int file, char *width) {
    //Current width
    unsigned int current_width = getWidth(file);
    //New width from parameter
    unsigned int new_width = atoi(width);
    //Save stdout
    int f_out = dup(1);
    dup2(file, 1);
    lseek(file, 0, SEEK_SET);
    //Write new width to file
    write(file,&new_width,sizeof(unsigned int));

    if (new_width < current_width) {
        int file2 = open("/tmp/tmpFile", O_CREAT | O_TRUNC | O_RDWR, 0666);
        if (dup2(file2, 1) == -1)
            perror("dup2");
        
	unsigned int height = getHeight(file);
        unsigned int nbobj  = getObject(file);
        
        write(file2,&new_width,sizeof(unsigned int));
        write(file2,&height,sizeof(unsigned int));
        write(file2,&nbobj,sizeof(unsigned int));
        //printf("\n");
	char CR='\n';
	write(file2,&CR,sizeof(char));

        lseek(file, 0, SEEK_SET);
        free(getLine(file));
        char *tmp = getLine(file);
        
        int i = 0;
        while (strcmp(tmp, "END") != 0) {
            if (i >= nbobj) {
                fprintf(stderr, "%s\n", tmp);
                int width_obj = atoi(tmp);
                if (width_obj <= new_width) {
                    printf("%s\n", tmp);
                }
            } else {
                printf("%s\n", tmp);
                fprintf(stderr, "%s\n", tmp);
            }
            free(tmp);
            tmp = getLine(file);
            i++;
        }
        free(tmp);
        printf("END\n");
        fflush(stdout);
        dup2(f_out, 1);
        copyAndTruncate(file2,file);
        close(file2);
    }
    dup2(f_out, 1);
}

void setHeight(int file, char *height) {
    int current_height = getHeight(file);
    int new_height = atoi(height);
    int f_out = dup(1);
    dup2(file, 1);
    lseek(file, sizeof(int), SEEK_SET);
    write(file, &new_height, sizeof(int));
    if (new_height < current_height) {
        lseek(file, 0, SEEK_SET);
        int file2 = open("/tmp/tmpFile", O_CREAT | O_TRUNC | O_RDWR, 0666);
        if (dup2(file2, 1) == -1)
            perror("dup2");

        unsigned int width = getWidth(file);
        unsigned int nbobj = getObject(file);
        
        write(file2,&width,sizeof(unsigned int));
        write(file2,&new_height,sizeof(unsigned int));
        write(file2,&nbobj,sizeof(unsigned int));
        printf("\n");
        lseek(file, 0, SEEK_SET);
        free(getLine(file));
        char *tmp = getLine(file);
        int i = 0;
        while (strcmp(tmp, "END") != 0) {
            if (i >= nbobj) {
                char *copy = (char *)malloc(strlen(tmp) + 1);
                if (copy == NULL) {
                  /* Handle error */
                }
                strcpy(copy, tmp);
                strtok(tmp, "\t");
                char *y = strtok(NULL, "\t");
                int height_obj = atoi(y);
                if (height_obj <= new_height) {
                    printf("%s\n", copy);
                    fprintf(stderr, "To tmpFile: %s\n", copy);
                }
            } else {
                printf("%s\n", tmp);
                fprintf(stderr, "%s\n", tmp);
            }
            free(tmp);
            tmp = getLine(file);
            i++;
        }
        free(tmp);
        printf("END\n");
        fflush(stdout);
        dup2(f_out, 1);
        copyAndTruncate(file2,file);
        close(file2);
    }
    dup2(f_out, 1);

}
