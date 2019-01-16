#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <time.h>
#include <pthread.h>

/* portul folosit */
#define PORT 2024

/* codul de eroare returnat de anumite apeluri */
extern int errno;
pthread_mutex_t lock;

char* printTrenuri(int mod); // mod = 0 afiseaza toate trenurile de azi, mod = 1 sosiri in urm ora, mod = 2 plecari din urm ora
char* adaugaIntarziere(char* msg);
char * writeRead(int client, char * msgrasp); 

int dateNotOk(char *);
int hourNotOk(char *);
int verify(char*, char*);
void writeNewUser(char*, char*);

int main (){
    struct sockaddr_in server;	// structura folosita de server
    struct sockaddr_in from;
    char msg[100];		//mesajul primit de la client
    char *msgrasp = NULL;        //mesaj de raspuns pentru client
    int sd;			//descriptorul de socket
	
    /* crearea unui socket */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1){
    	perror ("[server]Eroare la socket().\n");
    	return errno;
    }

    /* pregatirea structurilor de date */
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));

    /* umplem structura folosita de server */
    /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;
    /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    /* utilizam un port utilizator */
    server.sin_port = htons (PORT);

    /* atasam socketul */
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1){
    	perror ("[server]Eroare la bind().\n");
    	return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen (sd, 1) == -1){
    	perror ("[server]Eroare la listen().\n");
    	return errno;
    }

	/* servim in mod concurent clientii... */
    while (1){
    	int client;
    	int length = sizeof (from);

    	printf ("[server]Asteptam la portul %d...\n",PORT);
    	fflush (stdout);

    	/* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    	client = accept (sd, (struct sockaddr *) &from, &length);

    	/* eroare la acceptarea conexiunii de la un client */
    	if (client < 0){
			perror ("[server]Eroare la accept().\n");
			continue;
		}

    	int pid;
    	if ((pid = fork()) == -1){
    		close(client);
    		continue;
    	} else if (pid > 0){
    		// parinte
    		close(client);
    		while(waitpid(-1,NULL,WNOHANG));
    		continue;
    	} else if (pid == 0){
    		// copil
    		close(sd);
            
			// -----------------------------------------------------------------------------------------------------
			// bucla de comunicare
			
			// mesajul cu meniul
			char mesaj_meniu[] = "Alegeti una dintre comenzile urmatoare:\n1.Afisare mers\n2.Plecari in urm ora\n3.Sosiri in urm ora\n4.Adauga intarzieri\n5.exit\n\0";
			char mesaj_intarzieri[] = "Introduceti numarul trenului, plecare sau sosire si durata intarzierii (negativa daca ajunge mai devreme), separate prin virgula:\n\0";
			char mesaj_nodata[] = "Nu exista date de afisat!\n\0";
			char mesaj_sfarsit_comanda[] = "Puteti introduce in continuare comenzi, pentru a vedea lisa de comenzi apasati 0\n\0";
			int meniu = 0;	// 0=afisare menu, 1=mers, 2=plecari, 3=sosiri, 4=adaugare intarziere
			char* buff = 0;
			char mesaj_user[] = "Introduceti numele userului:\n\0";
			char mesaj_parola[] = "Introduceti parola:\n\0";
			char mesaj_gresit[] = "Userul sau parola sunt gresite, doriti sa mai incercati odata?(y/n)\n\0";
			char mesaj_newUser[] = "Doriti crearea unui cont(y/n)?\n\0";
			int loged = 0;
			char *userName = NULL, *password = NULL;
			char *rasp = 0, *newUser = 0;
			int yes = 1;
			while(1){

					while(!loged){

						if(yes == 1){
							// obtin numele userului si parola
							userName = writeRead(client, mesaj_user);
							password = writeRead(client, mesaj_parola);
							if(verify(userName, password)){
								loged = 1;
								
							}else{
								rasp = writeRead(client, mesaj_gresit);
								if(rasp[0] != 'y') yes = 0;
							}

						}else if(yes == 0){

							free(userName); free(password);

							printf("iesim\n");
							fflush (stdout);
						
							close (client);
							exit(0);
						}

						free(userName); free(password);
						userName = 0; password = 0;
						
					}

					
					// in functie de statusul curent trimit informatii catre client 
					
					// informatiile vor fi puse in buff
					if(meniu != -1){
						if(buff) free(buff);
						buff = 0;
					}

					if(meniu == 0){
						//afisare meniu
						buff = malloc(strlen(mesaj_meniu)+1);
						strcpy(buff, mesaj_meniu);					
					}else if(meniu == 1){
						// afisare trenuri				
						buff = printTrenuri(0);
						//meniu = 0;
					}else if (meniu == 2){
						// afisare plecari
						buff = printTrenuri(2);
						//meniu = 0;
					}else if (meniu == 3){
						// afisare sosiri
						buff = printTrenuri(1);
						//meniu = 0;
					}else if (meniu == 4){
						// adaugare intarziere
						buff = malloc(strlen(mesaj_intarzieri)+1);
						strcpy(buff, mesaj_intarzieri);
					}

					// daca mesajul este gol pun un mesaj generic 
					if(!buff){
						buff = malloc(strlen(mesaj_nodata)+1);
						sprintf(buff, mesaj_nodata);
					}

					if(meniu != 4 && meniu != 0){
						buff = realloc(buff, strlen(buff)+strlen(mesaj_sfarsit_comanda)+1);
						strcat(buff, mesaj_sfarsit_comanda);
					}
				


					// trimit mesajul catre client
					
					// 1. trimite lungimea mesajului 
					sprintf(msg,"%i",strlen(buff));
					if (write (client, msg, strlen(msg)) <= 0){
						perror ("[server]Eroare la write() catre client.\n");
						exit(0);
					}
			
					// 2. trimite textul mesajului 
					if (write (client, buff, strlen(buff)) <= 0){
						perror ("[server]Eroare la write() catre client.\n");
						exit(0);
					}

					free(buff);
					buff = 0;


					/* clientul returneaza un raspuns */
					/* citirea mesajului */
					bzero(msg,100);
					if (read (client, msg, 100) <= 0){
						perror ("[server]Eroare la read() de la client.\n");
						close (client);	/* inchidem conexiunea cu clientul */
						exit(0);
					}


					// se asteapta raspuns doar pentru optiunile 0 si 4, afisare meniu si introducere intarziere
					if(meniu == 0){
						// primeste de la client optiunea de meniu
						if (msg[0] == '5'){
							/* inchidem conexiunea, am terminat */
							printf("iesim\n");
							fflush (stdout);
						
							close (client);
							exit(0);
						}
						
					} else if(meniu == 4){
						// primeste de la client intarzierea
						pthread_mutex_lock(&lock);

						buff = adaugaIntarziere(msg);
						
						pthread_mutex_unlock(&lock);
						
					}

					if(meniu == 4) meniu = -1;
					else meniu = msg[0]-'0'; // transforma din ascii in int

					if(meniu<0 || meniu>4)
						perror("eroare meniu\n");
				
			
			}
    	}           
    }				
	return 0;
}				




char* printTrenuri(int mod){
	// mod = 0 trenuri azi, mod = 1 sosiri trenuri, mod = 2 plecari trenuri

	xmlDocPtr doc;
	xmlNodePtr nod;
	xmlNodePtr nodtren;

	doc = xmlParseFile("trenuri.xml");
	if(doc == NULL){
		perror("Parsing failed!\n");
		return 0;
	}
	nod = xmlDocGetRootElement(doc);
	if(nod == NULL){
		perror("empty document\n");
		xmlFreeDoc(doc);
		return 0;
	}


	char data[100], nume[100], sosire[100], plecare[100],  mod_sosire[100], mod_plecare[100], tren[1024];
	
	//char *return_text = NULL;
	int conditie, n, minute;
    xmlChar *key;

	char* msg;
	char txt[] = "Lista de trenuri\n\0";
	msg = malloc(strlen(txt)+1);	
	strcpy(msg, txt);
	char* buff;
    
	nodtren = nod->children;
	while(nodtren){
		nod = nodtren->children;
		bzero(data,100); bzero(nume, 100); bzero(sosire, 100); bzero(plecare, 100); bzero(tren, 1024);
		bzero(mod_plecare,100); bzero(mod_sosire,100);
		conditie = 0;
		while(nod){		
			if ((!xmlStrcmp(nod->name, (const xmlChar *)"data"))) {
				conditie = 1;
				key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
				strcpy(data, key);			
				if(dateNotOk(data)) conditie = 0;      
				xmlFree(key);
			}else if ((!xmlStrcmp(nod->name, (const xmlChar *)"nume"))) {
				key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
				strcpy(nume, key);
				xmlFree(key);
			}else if ((!xmlStrcmp(nod->name, (const xmlChar *)"sosire"))) {
				key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
				strcpy(sosire, key);
				if(mod == 1) if(hourNotOk(sosire)) conditie = 0;         
				xmlFree(key);
			}else if ((!xmlStrcmp(nod->name, (const xmlChar *)"plecare"))) {
				key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
				strcpy(plecare, key);
				if(mod == 2) if(hourNotOk(plecare)) conditie = 0;         
				xmlFree(key);
			}
			if(mod != 0){
				if (!xmlStrcmp(nod->name, (const xmlChar*)"mod_sosire")){
					key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
					minute = atoi(key);

					if(minute < 0) sprintf(mod_sosire, "soseste cu %d minute mai devreme!", 0-minute);
					else if (minute > 0) sprintf(mod_sosire, "soseste cu %d minute mai tarziu!", minute);
					else sprintf(mod_sosire, "ajunge la timp!");	

					xmlFree(key);			 
				}else if (!xmlStrcmp(nod->name, (const xmlChar*)"mod_plecare")){
					key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
					minute = atoi(key);

					if(minute < 0) sprintf(mod_plecare, "pleaca cu %d minute mai devreme!", 0-minute);
					else if (minute > 0) sprintf(mod_plecare, "pleaca cu %d minute mai tarziu!", minute);
					else sprintf(mod_plecare, "ajunge la timp!");	

					xmlFree(key);
				}
			}

			nod = nod->next;
		}

		if(conditie){
			if(mod == 1){
				n = sprintf(tren, "Numar tren: %s, data: %s, sosire: %s, plecare: %s, %s\n", nume, data, sosire, plecare, mod_sosire);
				msg = realloc(msg, strlen(msg)+n+1);
				strcat(msg, tren);
			}else if (mod == 2){
				n = sprintf(tren, "Numar tren: %s, data: %s, sosire: %s, plecare: %s, %s\n", nume, data, sosire, plecare, mod_plecare);
				msg = realloc(msg, strlen(msg)+n+1);
				strcat(msg, tren);
			}else{
				n = sprintf(tren, "Numar tren: %s, data: %s, sosire: %s, plecare: %s\n", nume, data, sosire, plecare);
				msg = realloc(msg, strlen(msg)+n+1);
				strcat(msg, tren);
			}
		
		}			

		nodtren = nodtren->next;
	}
	printf("in afisare tren: %s\n",msg);
	if(strcmp(msg, txt) == 0){
		free(msg);
		msg = NULL;
	}
	return msg;
}


//
char* adaugaIntarziere(char* msg){
	xmlDocPtr doc;
	xmlNodePtr nod;
	xmlNodePtr nodtren;
	char* tempPtr = NULL;
	doc = xmlParseFile("trenuri.xml");
	if(doc == NULL){
		perror("Parsing failed!\n");
		return 0;
	}
	nod = xmlDocGetRootElement(doc);
	if(nod == NULL){
		perror("empty document\n");
		xmlFreeDoc(doc);
		return 0;
	}
	
	char nume_tren[100], optiune[100], minute[100];
	char * raspuns = NULL;
	bzero(nume_tren,100); bzero(optiune,100); bzero(minute,100);

	int mod, conditie, succes = 0;
	xmlChar *key;

	tempPtr = strtok(msg, ",");
	strcpy(nume_tren, tempPtr);
	tempPtr = strtok(NULL, ",");
	strcpy(optiune, tempPtr);
	tempPtr = strtok(NULL, "\n");
	strcpy(minute, tempPtr);
	printf("..%s,%s,%s..\n",nume_tren,optiune,minute);

	if(!strcmp(optiune, "sosire")) mod = 1;
	else if(!strcmp(optiune, "plecare")) mod = 0;
	else {
		raspuns = malloc(strlen("Comanda incorecta, incercati din nou!\n")+1);
		strcpy(raspuns, "Comanda incorecta, incercati din nou!\n");
		return raspuns;
	}

	nodtren = nod->children;
	while(nodtren && succes == 0){ 
		// daca succes = 0 inseamna ca trenul cautat a fost gasit, nu mai e nevoie sa cautam si in celelalte
		nod = nodtren->children;
		conditie = 0;
		while(nod){		
			if ((!xmlStrcmp(nod->name, (const xmlChar *)"nume"))) {
				key = xmlNodeListGetString(doc, nod->xmlChildrenNode, 1);
				if(!strcmp(key, nume_tren)) {
					conditie = 1; succes = 1;
				}
			}
			if(conditie == 1){

				if (!xmlStrcmp(nod->name, (const xmlChar*)"mod_sosire") && mod == 1){

					xmlNodeSetContent(nod, (const xmlChar*)minute);

				}else if (!xmlStrcmp(nod->name, (const xmlChar*)"mod_plecare") && mod == 0){

					xmlNodeSetContent(nod, (const xmlChar*)minute);

				}
			}
			nod = nod->next;
		}
		nodtren = nodtren->next;
	}
	if(succes == 1){
		raspuns = malloc(strlen("Succes!\n")+1);
		strcpy(raspuns, "Succes!\n");
		xmlSaveFormatFile ("trenuri.xml", doc, 1);
	}else{
		raspuns = malloc(strlen("Trenul cu numele introdus nu exista!\n")+1);
		strcpy(raspuns, "Trenul cu numele introdus nu exista!\n");
	}

	return raspuns;
}



int dateNotOk(char * train_date){
    time_t rawtime;
	struct tm * tm_struct;

	char curent_date[100];
	int  day_curent, month_curent, year_curent;

    /* obtin ora curenta */
    time(&rawtime);
    tm_struct = localtime(&rawtime);
    day_curent = tm_struct->tm_mday;
    month_curent = tm_struct->tm_mon + 1;
    year_curent = tm_struct->tm_year + 1900;

	sprintf(curent_date, "%d/%d/%d", day_curent, month_curent, year_curent);
    return strcmp(train_date, curent_date);
}

int hourNotOk(char * h){

	int hour, min, hour_curent, minute_curent;
	char train_hour[100];
	strcpy(train_hour, h);

	time_t rawtime;
	struct tm * tm_struct;

	/* obtin ora curenta */
	time(&rawtime);
	tm_struct = localtime(&rawtime);
	hour_curent = tm_struct->tm_hour;
	minute_curent = tm_struct->tm_min;

	hour = atoi(strtok(train_hour, ":"));
	min = atoi(strtok(NULL, ":"));
	if( ( (hour == hour_curent)  && (min >= minute_curent) ) || ( ((hour - hour_curent) == 1) && ((min <= minute_curent) ) ) ) return 0; 
		
	return 1;
}

char * writeRead(int client, char * msgrasp){

	char len[100]; // lungimea mesajului
	char * msg = malloc(100); // mesajul primti de la client

	// trimit lungimea mesajului 
	sprintf(len,"%i",strlen(msgrasp));
	if (write (client, len, strlen(len)) <= 0) perror ("[server]Eroare la write() catre client.\n");
		
	// trimit mesajul  
	if (write (client, msgrasp, strlen(msgrasp)) <= 0) perror ("[server]Eroare la write() catre client.\n");
	
	// citesc mesajul de la client
	bzero(msg, 100);
	if (read (client, msg, 100) <= 0) perror ("[server]Eroare la read() de la client.\n");
	
	return msg;

}

int verify(char * nume, char * password){
	char * line = malloc(100);
	char * token;
	FILE * fptr;
	fptr = fopen("user.txt", "r");

	nume[strlen(nume)-1] = 0;
	password[strlen(password)-1] = 0;

	int ok1 = 0, ok2 = 0;
	while(fgets(line, 100, fptr)){
		
		token = strtok(line, " ");
		if(strcmp(nume,token) == 0 ) ok1 = 1;
		token = strtok(NULL, "\n");
		if(strcmp(password,token) == 0) ok2 = 1;
		if(ok1 == 1 && ok2 == 1) break;
		else{
			ok1 = 0; ok2 = 0;
		}
		bzero(line, 100);
	}
	free(line);
	printf("dar aici?\n");
	if(ok1 == 1 && ok2 == 1) return 1;
	else return 0;

}

void writeNewUser(char* nume, char* parola){
	char * line = malloc(100);
	char * token;
	FILE * fptr;
	fptr = fopen("user.txt", "a");

	nume[strlen(nume)-1] = 0;
	parola[strlen(parola)-1] = 0;

	fprintf(fptr, "\n%s %s", nume, parola);

}