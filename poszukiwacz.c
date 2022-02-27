#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<unistd.h>
#include<fcntl.h>
#include<time.h>
#include<signal.h>
#include<errno.h>
#include<string.h>
#include<sys/stat.h>
#include<math.h>

#define NANOSEC 1000000000L
#define FL2NANOSEC(f) {(long)(f),((f)-(long)(f))*NANOSEC}

int read_unit(char* unit){
        int m;
        if(!strcmp(unit, "Ki"))
                m =  1024;
        else if (!strcmp(unit, "Mi"))
                m = 1024 * 1024;

        return m;
}

int main(int argc,char* argv[]){
	struct timespec sleep_tm=FL2NANOSEC(0.2);
	
	if(argc != 2){
		fprintf(stderr,"Wrong number of arguments: expected 1, but given %d\n", argc-1);
		exit(11);
	}

	char *endptr;
	int n = strtol(argv[1], &endptr, 0);
	if( *endptr != '\0' ){
		n = n * read_unit(endptr);
	}

	struct stat info;

	if( fstat(0,&info) == -1){
		perror("Cannot check stdin");
		exit(11);
	}
	else if(!S_ISFIFO(info.st_mode)){
		fprintf(stderr,"Stdin is not connected to pipe");
		exit(11);
	}
	
	short x;    // zmienna przechowujaca aktualnie przeczytana liczbe
	unsigned int data[n];  // tablica z niepowtarzajacymi sie nowymi liczbami
	int d = 0;         // aktualny indeks tablica data[]
	
	int repeat = 0;    // licznik powtorzen
	int repeat_flag = 0;     

	struct record{
		unsigned int x;
		__pid_t pid;
	} r;

	r.pid = getpid();

	for(int i=0; i<n; i++){
		if(read(0, &x, 2) ==  -1){
			perror("Cannot read from stdin\n");
			exit(11);	
		}
		for(int j=0; j<d; j++){
			if(x == data[j]){
				repeat++;
				repeat_flag = 1;
				break;	
			}
		}
		if(!repeat_flag){
			data[d++] = x;
			r.x = x;
			while(write(1, &r, sizeof(struct record)) == -1){
				nanosleep(&sleep_tm, NULL);
			}
		}
		repeat_flag = 0;
	}
	int res = ceil((float)repeat / (repeat + d) * 10);
	
	return res;
}



