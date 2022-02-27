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
#include<sys/wait.h>

#define NANOSEC 1000000000L
#define FL2NANOSEC(f) {(long)(f),((f)-(long)(f))*NANOSEC}	

int read_unit(char* unit){    // funkcja interpretujaca jednostki
        int m;
        if(!strcmp(unit, "Ki"))
                m =  1024;
        else if (!strcmp(unit, "Mi"))
                m = 1024 * 1024;
        else {
                fprintf(stderr, "Wrong unit\n");
                exit(EXIT_FAILURE);
        }

        return m;
}

void write_raport(int fd, int pid, char* type){        // funkcja zapisujaca raport do pliku
	char raport_buff[60];
	struct timespec mono_time;

	clock_gettime(CLOCK_MONOTONIC, &mono_time);
	snprintf(raport_buff, 60, "Sec: %ld nanosec: %ld %s %d\n", mono_time.tv_sec, mono_time.tv_nsec, type, pid);
	write(fd, &raport_buff, strlen(raport_buff));
}

void write_success(int fd , unsigned int x, pid_t pid){       // funkcja zapisujaca sukces do pliku
	int size = sizeof(pid_t);

	if(lseek(fd, size * x - size, SEEK_SET) == -1){
		perror("Cannot go through success file");
		exit(EXIT_FAILURE);
	}

	pid_t check;

	if((read(fd, &check, size) == -1)){
		perror("Cannot read from success report file");
		exit(EXIT_FAILURE);
	}

	off_t offset = (off_t)-1 * size;

	if(check == 0 ){
		if((lseek(fd, offset, SEEK_CUR)) == -1){
			perror("Cannot go through success file");
			exit(EXIT_FAILURE);
		}

		if(write(fd, &pid, size) == -1){
			perror("Cannot write to success raport file");
			exit(EXIT_FAILURE);
		}
	}
}


int main(int argc,char* argv[]){
	struct timespec sleep_tm=FL2NANOSEC(0.48);

	int opt;
	opterr = 0;

	char options[]={'d','s','w','f','l','p'};
	int nr_options = sizeof(options);
	int flags[nr_options];
	memset(flags, 0, nr_options * sizeof(int));
	char* values[6];

	while((opt = getopt(argc, argv, "d:s:w:f:l:p:")) != -1 ){
		switch(opt){
			case 'd':{
					 flags[0] = 1;
					 values[0] = optarg;
					 break;
				 }
			case 's':{
					 flags[1] = 1;
					 values[1] = optarg;
					 break;
				 }

			case 'w':{
					 flags[2] = 1;
					 values[2] = optarg;
					 break;
				 }
			case 'f':{
					 flags[3] = 1;
					 values[3] = optarg;
					 break;
				 }
			case 'l':{
					 flags[4] = 1;
					 values[4] = optarg;
					 break;
				 }
			case 'p':{
					 flags[5] = 1;
					 values[5] = optarg;
					 break;
				 }
			case '?':{
					printf("Option -%c needs an argument\n",optopt);
					exit(EXIT_FAILURE);
				}
		}
	}
	

	for(int i=0; i<nr_options; i++){
		if(flags[i] == 0){
			fprintf(stderr,"Parametr %c not specified\n", options[i]);
			exit(EXIT_FAILURE);
		}
	}


	int zrodlo = open(values[0], O_RDONLY);
	if(zrodlo == -1){
		perror("Cannot open source file");
		exit(EXIT_FAILURE);
	}

	char* endptr;
	int wolumen;
       	if((wolumen= strtol(values[1], &endptr, 0)) == 0 ){
		fprintf(stderr, "Wrong type of argument in option -%c\n", options[1]);
		exit(EXIT_FAILURE);
	}
	if( *endptr != '\0'){
		wolumen = wolumen * read_unit(endptr);
	}

	endptr = NULL;
	int blok;
       	if((blok= strtol(values[2], &endptr, 0)) == 0){
		fprintf(stderr, "Wrong type of argument in option -%c\n", options[2]);
		exit(EXIT_FAILURE);
	}
	if( *endptr != '\0'){
		blok = blok * read_unit(endptr);
	}


	int sukcesy = open(values[3], O_RDWR | O_TRUNC);
	if(sukcesy == -1){
		perror("Cannot open success raport file");
		exit(EXIT_FAILURE);
	}

	pid_t zero = 0;
	int n = 65535;               // liczba wszystkich mozliwych liczb

	for(int i=0; i<n; i++){
		if(write(sukcesy, &zero, sizeof(pid_t)) < 0){
			perror("Cannot write to success raport file");
			exit(EXIT_FAILURE);
		}
				
	}

	int raporty = open(values[4], O_RDWR | O_TRUNC);
	if(raporty == -1){
		perror("Cannot open raport file");
		exit(EXIT_FAILURE);
	}
	
	endptr = NULL;
	int prac;
       	if(((prac= strtol(values[5], &endptr, 0)) == 0) | (*endptr != '\0')){
		fprintf(stderr, "Wrong type of argument in option -%c\n", options[5]);
		exit(EXIT_FAILURE);
	}
	
	if(blok * prac > wolumen){
		fprintf(stderr, "Value for option -%c is too big\n", options[2]);
		exit(EXIT_FAILURE);
	}

	int pipefd_r[2];   // potok do czytania 
	int pipefd_w[2];   // potok do pisania

	if(pipe(pipefd_r) == -1){
		perror("Cannot create pipe");
		exit(EXIT_FAILURE);
	}
	if(pipe(pipefd_w) == -1){
		perror("Cannot create pipe");
		exit(EXIT_FAILURE);
	}
	
	if((fcntl(pipefd_r[0], F_SETFL, O_NONBLOCK) < 0) || (fcntl(pipefd_r[1],F_SETFL,O_NONBLOCK) < 0)){
		perror("Cannot make non-block mode");
		exit(EXIT_FAILURE);
	}

	if((fcntl(pipefd_w[0], F_SETFL, O_NONBLOCK) < 0) || (fcntl(pipefd_w[1],F_SETFL,O_NONBLOCK) < 0)){
		perror("Cannot make non-block mode");
		exit(EXIT_FAILURE);
	}
	
	unsigned int buff[wolumen];   // tablica danych ze zrodla
	int read_source = 0;    // liczba wczytanych danych ze zrodla

	if((read_source = read(zrodlo, &buff, 2 * wolumen)) != 2 * wolumen){

		if(read_source == -1){
			perror("Cannot read from source file");
		}
		else {
			fprintf(stderr, "Source file have not enough data: missing %d bytes\n", 2 * wolumen - read_source);
		}
		exit(EXIT_FAILURE);
	}


	int write_wolumen = 0;  // liczba wpisanych do pipe'a danych
	int curr_write_wolumen = 0;   // liczba wpisanych do pipe'a danych w aktualnym kroku
	
	int success_read = 0;        // liczba danych wczytanych od potomkow
	int idx = 0;                 // indeks sprawdzajacy ile osiagniec zostalo uzyskanych
	struct record{
		unsigned int x;
		__pid_t pid;
	} r;
	
	int wait;
	int status; 
	int success_dead = 0;       // zmienna informujaca czy jakis potomek umarl

	pid_t pids[prac];       // tablica pidow potomkow
	memset(pids, 0, prac * sizeof(pid_t));
	
	int counter = 0;        // liczba danych, ktore zostaly juz rozdanych do potomkow

	int alive = 0;          // zmienna informujaca czy jakis potomek jest zywy

	while(1){
		if(write_wolumen !=  2 * wolumen) {
			if((curr_write_wolumen = write(pipefd_w[1], &buff, 2 * wolumen)) != -1){
				write_wolumen += curr_write_wolumen;
			} else if(errno != EAGAIN){
					perror("Cannot write to pipe");
				}
			}
		
		r.x = 0;
		r.pid = 0;

		success_read = read(pipefd_r[0], &r, sizeof(struct record));

		if (success_read < 0 && errno != EAGAIN){
			perror("Cannot read from pipe");
			exit(EXIT_FAILURE);
		} else if (success_read > 0) {
			idx++;
			
			write_success(sukcesy, r.x, r.pid);
		}

		for(int i=0; i<prac; i++){           // petla sprawdzajaca czy potomkowie umarli
			if(pids[i] < 1) 
				continue;
			if((wait = waitpid(pids[i], &status, WNOHANG)) == -1){
				perror("Cannot check status of child");
				pids[i] = -1;
				continue;
			}

			if(wait == 0)
				continue;
			
			success_dead = 1;
			
			write_raport(raporty, pids[i], "zakonczenie");

			if(!WIFEXITED(status) || WEXITSTATUS(status) > 10){
				pids[i] = -1;
				continue;
			}

			pids[i] = 0;

		}

		for(int i=0; i<prac; i++){         // petla tworzaca nowych potomkow
			if(pids[i] != 0) 
				continue;
			if(counter >= wolumen)
				break;
			if(idx > 0.75 * n)        // koniec tworzenia nowych potomkow
				break;
			
			char arg[10];
			if( wolumen - counter < blok){            // sprawdzanie czy pozostalo danych na jeden blok
				snprintf(arg, 10, "%d", wolumen - counter);
				values[2] = arg;
				counter = wolumen;
			} else {
				counter += blok;
			}

			if((pids[i]=fork()) == 0 ){
				if(dup2(pipefd_w[0], 0) == -1){ 
					perror("Cannot duplicate a pipe");
					exit(EXIT_FAILURE);
				}
				if(dup2(pipefd_r[1], 1) == -1){
					perror("Cannot duplicate a pipe");
					exit(EXIT_FAILURE);
				}

				close(pipefd_r[0]);
				close(pipefd_w[1]);
				
				execl("poszukiwacz","poszukiwacz", values[2], NULL);
			} else if(pids[i] < 0)
				perror("Cannot create a child");

			write_raport(raporty, pids[i], "utworzenie");
		}
		if(success_read < 1 && success_dead == 0)
			nanosleep(&sleep_tm, NULL);
		
		alive = 0;
		for(int i=0; i<prac; i++){
			if(pids[i] > 0){
				alive = 1;
			}
		}
		
		if( alive == 0 && success_read == -1){          // warunek konca petli
			break;
		}

		success_dead = 0;

	}
	exit(EXIT_SUCCESS);	
}



