//
//      vip -
//              Copy any number of source code files into a single file for editing.
//      When any changes are saved from vi, a process rapidly parses the single file,
//      finds files that need changing, and writes out the individual files.
//
//      At the same time, if any of the individual source files is changed by another 
//      process (like Xcode), then the single file is regenerated.
//
//      This makes it insanely fast and easy to search and edit hundreds of source files
//      as a single file of 0..500,000 lines of C, C++, Swift, etc.
//
//      Sincere thanks for Chris Pirazzi for his help and advice.
//
//      Paul Haeberli 2016
//
//      SERIOUS WARNING: Please only use on source code that has been backed up, and is 
//      under source control.  There may be a few bugs in this implementation.  I am 
//      very afraid of this damaging your source code! 
//
//
//      to compile:
//
//          cc vip.c -o vip
//
//      usage:
//
//         vip app.h app/*.h app/*.c libgfx/*.h libgfx/*.c libios/*.h libios/*.m
//
//
#include "fcntl.h"
#include "sys/wait.h"
#include "sys/stat.h"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "stdlib.h"
#include "string.h"

#define MAXTEXTLINE     (8*4096)
#define TEMPBUFSIZE     (1024*1024)
static char *flagdirname;
static char *mergetempname;
static unsigned char tmpbuf[TEMPBUFSIZE];

void errorexit(const char *msg, const char *filename)
{
    fprintf(stderr, "***\n***\n***\n");
    if(filename) 
        fprintf(stderr, "***    vip: Fatal Error: %s filename: [%s]\n", msg, filename);
    else
        fprintf(stderr, "***    vip: Fatal Error: %s\n", msg);
    fprintf(stderr, "***    vip: mergefile path is [%s]\n", mergetempname);
    fprintf(stderr, "***\n***\n***\n");
    exit(1);
}

void fatalexitsplit(int n)
{
    fprintf(stderr, "***\n***\n***\n");
    fprintf(stderr, "***    vip: fatal Error %d\n", n);
    fprintf(stderr, "***    vip: mergefile path is [%s]\n", mergetempname);
    fprintf(stderr, "***\n***\n***\n");
    exit(1);
}

int binarydata(unsigned char *buf, int n) 
{
    while(n--) {
        char c = *buf++;
        if(!((c == 9) || (c==10) || (c==13) || (c>=32 && c<=126)))
            return 1;
    }
    return 0;
}

int writefile(FILE *outf, const char *filename, int fileno, int nfiles)
{
    FILE *inf = fopen(filename, "r");
    if(!inf) 
        errorexit("writefile: can't open input file", filename);

    fprintf(outf,"// [%s] Start Fileno: [%d:%d] ********************************************************** VIP\n", filename, fileno, nfiles);

    while(1) {
        int nr = (int)fread(tmpbuf, 1, TEMPBUFSIZE, inf);
        if(nr<=0)
            break;
        if(binarydata(tmpbuf, nr)) {
            fprintf(stderr, "vip: Error [%s] is a binary file\n", filename);
            fflush(stderr);
            exit(1);
        }
        int nw = (int)fwrite(tmpbuf, 1, nr, outf);
        if(nw != nr) {
            fprintf(stderr, "vip: copy Error: read %d write %d\n", nr, nw);
            fclose(inf);
            exit(1);
        }
    }
    fclose(inf);

    fprintf(outf,"// [%s] End Fileno: [%d:%d] ********************************************************** VIP\n", filename, fileno, nfiles);

    return 1;
}

void filetime(const char *filename, time_t *sec, long *nsec)
{
    struct stat buf;
    int f = open(filename, O_RDONLY);
    if(f<0) {
        *sec = 0;
        *nsec = 0;
        fprintf(stderr, "filetime: open failed for [%s]\n", filename);
    }
    fstat(f, &buf);
    close(f);
#ifdef LINUX
    *sec = buf.st_mtim.tv_sec;
    *nsec = buf.st_mtim.tv_nsec;
#else
    *sec = buf.st_mtimespec.tv_sec;
    *nsec = buf.st_mtimespec.tv_nsec;
#endif
}

void merge(char *mergefilename, char **filenames, time_t *filesec, long *filensec, int nfiles)
{
    FILE *outf = fopen(mergefilename, "w");
    if(!outf)
        errorexit("can't open output file", mergefilename);
    for(int i=0; i<nfiles; i++) {
        writefile(outf, filenames[i], i+1, nfiles);
        filetime(filenames[i], filesec++, filensec++);
    }
    fclose(outf);
}

int changed(const char *filename)
{
    return 1;
}

int strstartswith(const char *str, const char *pattern)
{
    char *cptr;

    cptr = strstr(str, pattern);
    if(cptr == str)
        return 1;
    return 0;
}

char *rindex(const char *s, int c)
{
    const char *ret;

    ret = 0;
    while(*s) {
        if(*s == c)
            ret = s;
        s++;
    }
    return (char *)ret;
}

char *lindex(const char *s, int c)
{
    while(*s) {
        if(*s == c)
            return (char *)s;
        s++;
    }
    return 0;
}

char *datafromfile(const char *filename, int *len)
{   
    struct stat buf;
    char *data;
        
    int f = open(filename, O_RDONLY);
    if(f<0) {
        fprintf(stderr, "datafromfile: open failed for [%s]\n", filename);
        *len = -1;
        return 0;
    }
    fstat(f, &buf);
    *len = buf.st_size;
    if(buf.st_size<=0) {
        *len = 0;
        return 0;
    }
    data = (char *)malloc((int)buf.st_size);
    int nr = (int)read(f, data, buf.st_size);
    if(nr != buf.st_size) {
        fprintf(stderr, "datafromfile: data from file wanted %d bytes got %d\n", (int)buf.st_size, nr);
        *len = -1;
        return 0;
    }
    close(f);
    return data;
}   

void datatofile(const char *filename, char *buf, int len)
{   
    FILE *f = fopen(filename, "w");
    int nw = fwrite(buf, 1, len, f);
    if(nw != len) {
        fprintf(stderr, "datatofile: datatofile wrote %d instead of %d bytes\n", nw, len);
        perror("WRITE: ");
        exit(1);
    }
    fclose(f);
#ifdef NOTDEF
    int f = open(filename, O_CREAT|O_TRUNC, 0666);
    if(f == -1)
        errorexit("datatofile fopen failed", filename);
    int nw = (int)write(f, buf, len);
    if(nw != len) {
        fprintf(stderr, "desc is %d\n", f);
        fprintf(stderr, "datatofile: datatofile wrote %d instead of %d bytes\n", nw, len);
        perror("WRITE: ");
        exit(1);
    }
    close(f);
#endif
}   

int filesame(const char *filename1, const char *filename2) 
{
    int len1;
    int len2;
    char *data1 = datafromfile(filename1, &len1);
    if(len1<0) 
        errorexit("datafromfile failed 1", filename1);
    char *data2 = datafromfile(filename2, &len2);
    if(len2<0)
        errorexit("datafromfile failed 2", filename2);
    if((len1 == 0) && (len2 == 0))
        return 1;
    if(len1 != len2) {
        free(data1);
        free(data2);
        return 0;
    }
    char *cptr1 = data1;
    char *cptr2 = data2;
    while(len1--) {
        if(*cptr1++ != *cptr2++) {
            free(data1);
            free(data2);
            return 0;
        }
    }
    free(data1);
    free(data2);
    return 1;
}

int datasame(char *data1, char *data2, int len1, int len2)
{
    if(len1 != len2)
        return 0;
    while(len1--) {
        if(*data1++ != *data2++)
            return 0;
    }
    return 1;
}

void split(const char *mergefilename, time_t *filesec, long *filensec, int nfiles)
{
    char buf[MAXTEXTLINE];

    // try to protect against reading changing file
    int nrep = 0;
    int len1, len2;
    char *data1, *data2;
    while(1) {
        data1 = datafromfile(mergefilename, &len1);
        if(len1<0)
            errorexit("split: datafromfile failed 1", mergefilename);
        data2 = datafromfile(mergefilename, &len2);
        if(len2<0)
            errorexit("split: datafromfile failed 2", mergefilename);
        if(datasame(data1, data2, len1, len2))
            break;
        if(data1)
            free(data1);
        if(data2)
            free(data2);
        nrep++;
        if(nrep == 20)
            errorexit("split: Error geting consitent data", 0);
    }
    if(len1>0)
        datatofile(mergetempname, data1, len1);
    if(data1)
        free(data1);
    if(data2)
        free(data2);

    // parse file and write out files
    FILE *inf = fopen(mergefilename, "r");
    if(!inf)
        errorexit("split: can't open input file", mergefilename);
    int fno = 1;
    while(fgets(buf, MAXTEXTLINE, inf)) {
        if(strstartswith(buf,"//") && strstr(buf,"Start") && strstr(buf,"Fileno:") && strstr(buf,"********* VIP")) {
            char *fnamestart = lindex(buf, '[');
            if(!fnamestart)
                fatalexitsplit(1);
            fnamestart += 1;
            char *fnameend = lindex(fnamestart, ']');
            if(!fnameend)
                fatalexitsplit(2);
            *fnameend = 0;
            char *fnostart = lindex(fnameend+1, '[');
            if(!fnostart)
                fatalexitsplit(3);
            fnostart += 1;
            char *fnoend = lindex(fnostart, ']');
            if(!fnoend)
                fatalexitsplit(4);
            *fnoend = 0;
            char *filename = fnamestart;
            char *fileno = fnostart;
            char *colon = lindex(fileno,':');
            if(!colon)
                fatalexitsplit(5);
            *colon = 0;
            int thisfileno = atoi(fileno);
            int totfiles = atoi(colon+1);
            if(thisfileno != fno) {
                fprintf(stderr, "***\n***\n***\n");
                fprintf(stderr, "***    vip: Error: thisfileno is %d, it should be %d\n", thisfileno, fno);
                fprintf(stderr, "***    vip: mergefile path is [%s]\n", mergetempname);
                fprintf(stderr, "***\n***\n***\n");
                exit(1);
            }
            if(totfiles != nfiles) {
                fprintf(stderr, "***\n***\n***\n");
                fprintf(stderr, "***    vip: Error: totfiles is %d, it should be %d\n", totfiles, nfiles);
                fprintf(stderr, "***    vip: mergefile path is [%s]\n", mergetempname);
                fprintf(stderr, "***\n***\n***\n");
                exit(1);
            }
            char realfilename[4096];
            char changedfilename[4096];
            sprintf(realfilename, "%s",filename);
            sprintf(changedfilename, "%sT",filename);
            FILE *outf = fopen(changedfilename, "w"); 
            if(!outf)
                errorexit("split: can't open output file", changedfilename);
            while(fgets(buf, MAXTEXTLINE, inf)) {
                if(strstartswith(buf,"//") && strstr(buf,"End") && strstr(buf,"Fileno:") && strstr(buf,"********* VIP")) 
                    break;
                fputs(buf, outf);
            }
            fclose(outf);
            if(!filesame(realfilename, changedfilename)) {
                unlink(realfilename);
                rename(changedfilename, realfilename);
                int findex = fno-1;
                filetime(realfilename, filesec+findex, filensec+findex);
            } else {
                unlink(changedfilename);
            }
            fno++;
        }
    }
    fclose(inf);
    unlink(mergetempname);
}

int fileexists(const char *filename)
{
    FILE *inf;

    inf = fopen(filename, "r");
    if(!inf)
        return 0;
    fclose(inf);
    return 1;
}

int main(int argc, char **argv)
{
    int pid;
    time_t mergesec;
    long mergensec;
    time_t nowmergesec;
    long nowmergensec;
    time_t fsec;
    long fnsec;
    time_t *filesec;
    long *filensec;

    if(argc<2) {
        fprintf(stderr, "usage: vip app.h app/*.h app/*.c libgfx/*.h libgfx/*.c libios/*.h libios/*.m\n");
        exit(1);
    }
    flagdirname = strdup("/tmp/vip.lockdir.XXXXXX");
    mergetempname = strdup("/tmp/vip.pool.XXXXXX");
    mktemp(flagdirname);
    mktemp(mergetempname);

    int nfiles = argc-1;
    filesec = (time_t *)malloc(nfiles*sizeof(time_t));
    filensec = (long *)malloc(nfiles*sizeof(long));
    char *mergefilename = "/tmp/t.p";
    merge(mergefilename, argv+1, filesec, filensec, nfiles);
    filetime(mergefilename, &mergesec, &mergensec);

    char cmd[1024];
    sprintf(cmd, "vi /tmp/t.p");

    mkdir(flagdirname, 0777);
    pid=fork();
    if(pid) {
        int loc;
        system(cmd);
        rmdir(flagdirname);
        while(wait(&loc) == -1) {
        }
        filetime(mergefilename, &nowmergesec, &nowmergensec);
        if((nowmergesec != mergesec) || (nowmergensec != mergensec))
            split(mergefilename, filesec, filensec, nfiles);
        unlink(mergefilename);
        return 0;
    } else {
        while(fileexists(flagdirname)) {
            sleep(1);
            filetime(mergefilename, &nowmergesec, &nowmergensec);
            if((nowmergesec != mergesec) || (nowmergensec != mergensec)) {
                split(mergefilename, filesec, filensec, nfiles);
                filetime(mergefilename, &mergesec, &mergensec);
            }
            int nchanged = 0;
            for(int i=1; i<argc; i++) {
                int findex = i-1;
                filetime(argv[i], &fsec, &fnsec);
                if((fsec != filesec[findex]) || (fnsec != filensec[findex]))
                    nchanged++;
            }
            if(nchanged) {
                merge(mergefilename, argv+1, filesec, filensec, nfiles);
                filetime(mergefilename, &mergesec, &mergensec);
            }
        }
        return 0;
    }
}
