// Andrey Smotrov (14.07.2016) (tdr2004 @ list.ru)
// command: gcc -O3 -lpthread filesrv.c ini/ini.c -o filesrv

//#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
//#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
//#pragma GCC diagnostic ignored "-Wunused-result"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "str_functions.c"		// string_t and InStr, Split, MkDir functions
#include "ini/ini.h"

#include "stream.c"

#define CONNMAX 4096



void CloseAll();
void SendToAll(char *);
int ExecCommand(int,char *);
void *ClientWork(void *);
int startServer();
int accepting();
void log_print(char *);

// SETTINGS
int srv_port = 0; // PORT
char *srv_host = "0.0.0.0"; //IP
int buffer_size = 65536 ;
int out_buffer_size = 65536 ;

//char *akey = "4998017A";		// Ключ авторизации администратора
char *ukey = "28C88C04";		// Ключ авторизации
char *ekey = "14F2FBAD";		// Ключ шафрования файлов

char *storage = "/mnt/";		// Директория для хранения

int remove_broken = 1;			// Удалять недокаченные при разрыве
int keep_connections = 1;		// Держать соединение после загрузки файла
int max_timeout = 10;			// Максимальное время ожидания, после которого клиент кикается
int authorized_only = 1;		// Допускать только авторизованных клиентов
int authorized_timeout = 1;		// Включает время на авторизацию. Если не успел - кикать. использует max_timeout.
int make_dir = 1;				// Создавать папки
int allow_time_mod = 1;			// Разрешить изменять время файла
int allow_download = 1;			// Разрешить скачивание файла
char *logfile = "filesrv.log";				// Файл логов сервера
char *configfile = "filesrv.conf";				// Файл логов сервера
int log_size = 1048576;
int log_events = 1;
int file_buffer = 65536;
//


int client[CONNMAX];			// Client socket
char *clientIP[CONNMAX];		// Client IP
int state[CONNMAX];				// Client Status
int admin[CONNMAX];				// Admin Status
time_t last_time[CONNMAX];		// Client timeout
FILE *fp[CONNMAX];				// Connection's current file
char *fname[CONNMAX];			// Connection's current file name
int filest[CONNMAX];			// Connection's current file status
int srv_socket;					// Server socket

//FILE *flog;
char logbuffer[256];
char time_buffer[26];

pthread_t sck_thrd[CONNMAX];	// Client's Threads


static int conf_handler(const char* section, const char* name,
                   const char* value)
{
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	//printf("cfg: [%s]  %s = '%s'\n",section,name,value);
    if (MATCH("network", "srv_port")) {
		srv_port = atoi(value);
    } else if (MATCH("network", "srv_host")) {
		srv_host = strdup(value);
    } else if (MATCH("network", "buffer_size")) {
		buffer_size = atoi(value);
    } else if (MATCH("network", "out_buffer_size")) {
		out_buffer_size = atoi(value);

    } else if (MATCH("secure", "ukey")) {
		ukey = strdup(value);
    } else if (MATCH("secure", "ekey")) {
		ekey = strdup(value);

    } else if (MATCH("storage", "storage")) {
		storage = strdup(value);

    } else if (MATCH("server_sets", "remove_broken")) {
		remove_broken = atoi(value);
	} else if (MATCH("server_sets", "keep_connections")) {
		keep_connections = atoi(value);
	} else if (MATCH("server_sets", "max_timeout")) {
		max_timeout = atoi(value);

	} else if (MATCH("server_sets", "authorized_only")) {
		authorized_only = atoi(value);
	} else if (MATCH("server_sets", "authorized_timeout")) {
		authorized_timeout = atoi(value);
	} else if (MATCH("server_sets", "make_dir")) {
		make_dir = atoi(value);
	} else if (MATCH("server_sets", "allow_time_mod")) {
		allow_time_mod = atoi(value);
	} else if (MATCH("server_sets", "allow_download")) {
		allow_download = atoi(value);
	} else if (MATCH("server_sets", "log_size")) {
		log_size = atoi(value);
	} else if (MATCH("server_sets", "logfile")) {
		logfile = strdup(value);
	} else if (MATCH("server_sets", "log_events")) {
		log_events = atoi(value);

    } else if (MATCH("server_sets", "file_buffer")) {
		file_buffer = atoi(value);


    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}
int load_config(){
    if (ini_parse(configfile, conf_handler) < 0) {
		sprintf(logbuffer,"Can't load 'filesrv.conf'. Sets default."); log_print(logbuffer);
        return 1;
    }
}

void log_print(char *logmsg) {
	struct tm* tm_info;
	time_t timer;
	FILE *flog;
	time(&timer);
	tm_info = localtime(&timer);
	strftime(time_buffer, 26, "%Y:%m:%d %H:%M:%S ", tm_info);
	int f_size;

	if (strlen(logfile) != 0) {
		if (strncmp(logfile,"*",1) == 0) {
			flog = fopen("filesrv.log", "a+");
		}else{
			flog = fopen(logfile, "a+");
		}

		// Check Log Size
		//fseeko(flog, 0 , SEEK_END) != 0;
		if (flog != NULL) {
			fseeko(flog, 0 , SEEK_END);
			f_size = ftello(flog);
			if (f_size >= log_size) {
				fclose(flog);
				if (strncmp(logfile,"*",1) == 0) {
					flog = fopen("filesrv.log", "w");
				}else{
					flog = fopen(logfile, "w");
				}
				if (flog != NULL) fprintf(flog, "%s Log cleared\r\n",time_buffer);
			}

			if (flog != NULL) {
				fprintf(flog, "%s %s\r\n",time_buffer,logmsg);
				fclose(flog);
			}
		}
	}else{
		fprintf(stderr, "%s %s\r\n",time_buffer,logmsg);
	}

}


void CloseAll() {
	int i;
	for (i=0;i<CONNMAX;i++)
	{
		if (client[i] != -1) {
			shutdown (client[i], SHUT_RDWR);
			close(client[i]);
			client[i]=-1;
		}
	}
}
void SendToAll(char data[]) {
	int i;
	for (i=0;i<CONNMAX;i++)
	{
		if (client[i] != -1) {
			write(client[i], data, strlen(data));
		}
	}
}


/*
int64_t S64(const char *s) {
  int64_t i;
  char c ;
  int scanned = sscanf(s, "%" SCNd64 "%c", &i, &c);
  if (scanned == 1) return i;
  if (scanned > 1) {
    // TBD about extra data found
    return i;
    }
  // TBD failed to scan;
  return 0;
}
*/
void Decode_File(char *data,int length,int pos){
	int i,k = (pos % 8);

    for(i=0; i<length; i++)
    {
        data[i]=data[i]^ekey[(i+pos) % 8];
    }
}

void *ClientWork(void *arg) {
	int a = (intptr_t)arg;

	//sprintf(logbuffer,"client thread created: %s",clientIP[a]); log_print(logbuffer);

	int rcvd;
	int sent;
	char mesg[buffer_size];
	//char wrtbuff[file_buffer];
	char *fbuff = (char*)malloc(out_buffer_size);
	int buff_len;

	char *file_buff;
	file_buff=0;
	string_t val[99];
	string_t val2[9];
	string_t dttm[12];

	char cam_MAC[13];
	char cam_NUMs[3];
	int cam_NUM;

	time_t dwnl_time;
	time_t ctime;
	float dif;
	int KBpS, KB;

	char *f_path;
	char *m_time;
	//int64_t f_size = 0;
	int f_size = 0;
	//int file_size;
	int i, l, l2,l3; char *delm = " ";
	int  ifz, ipth;

	// Downloads
	int dl_offset = 0;
	int dl_buffsize = 0;
	int dl_simple = 0;

	if (authorized_only == 0) {
		state[a] = 0;		// Можно сразу загружать файлы
	}else{
		state[a] = -1;		// Ждем авторизации
	}

	char path[256], path2[256], t_path[256], scmd[256];
	int seekpos;
	int buffpos,wrcnt,blocki;
	int encode;
	int first_rcv;

	//int filest=0;
	filest[a]=0;

	while(1){
		if (client[a] == -1) goto labelbreak;
		rcvd = recv(client[a], mesg, buffer_size, 0);
		if (rcvd<0) {
			//printf(("recv() error\n"));
			sprintf(logbuffer,"recv error < 0"); log_print(logbuffer);
			if ((fp[a] != NULL) && (filest[a] == 1)) { fclose (fp[a]); filest[a] = 0; }
			goto labelbreak;

		}else if (rcvd==0){

			if (log_events==1) {sprintf(logbuffer,"Disconnected: %s",clientIP[a]); log_print(logbuffer);}
			if ((fp[a] != NULL) && (filest[a] == 1)) { fclose (fp[a]); filest[a] = 0; }
			goto labelbreak;

		}else{
			// Add NULL at the end of the string
			if (rcvd < buffer_size)	mesg[rcvd]=0;

			//sprintf(logbuffer,"recvd bytes=%d",rcvd); log_print(logbuffer);

			// RECEIVING FILE DATA / Получаем файл
			if ((state[a] == 1)){
				if (fp[a] != NULL) {
					filest[a] = 1;
					time(&last_time[a]); // recv last time

					// Декодируем данные
					switch (encode)
					{
					case 0:
						break;
					case 1:
						Decode_File(mesg,rcvd,seekpos);
						break;
					}

					memcpy(file_buff+seekpos, mesg, rcvd);

					// Ставим seekpos на следующий блок данных и проверяем на завершение файла
					seekpos += rcvd;
					if (seekpos >= f_size) {

						// Отправляем сообщение о завершении
						write(client[a], "DONE", 4);

						int rrt;
						rrt = SendStream(file_buff, f_size, cam_MAC, cam_NUM);

						// Пишем на диск
						fseek( fp[a] , 0 , SEEK_SET );
						rrt = fwrite(file_buff, f_size, 1, fp[a]);
						if (rrt != 1) {printf ("write error\n");}
						if (file_buff!=0) {free(file_buff); file_buff=0;}

						/*
						// Записываем последний кусок блока
						buffpos = seekpos % file_buffer;
						if (buffpos>0){
							fseek(fp[a] , (blocki*file_buffer) , SEEK_SET );
							rrt = fwrite(wrtbuff,buffpos,1,fp[a]);
							if (rrt != 1) {printf ("write error\n");}
						}
						*/

						// DONE
						state[a] = 0;

						// меняем дату
						if (allow_time_mod == 1) {
							if (strlen(m_time) > 0) {
								l3= strsplit(dttm,m_time,":");
								if (l3==6) {
									time_t tmr;
									struct tm *y2k;
									struct utimbuf new_times;
									y2k=malloc(sizeof(struct tm));
									//time ( &tmr); y2k = localtime ( &tmr );
									y2k->tm_year = -1900+ atoi(dttm[0].s);y2k->tm_mon = -1+atoi(dttm[1].s);y2k->tm_mday =atoi(dttm[2].s);
									y2k->tm_hour = atoi(dttm[3].s); y2k->tm_min = atoi(dttm[4].s); y2k->tm_sec = atoi(dttm[5].s);
									tmr=mktime(y2k);
									new_times.actime=tmr;
									new_times.modtime=tmr;

									utime(path, &new_times); // t_path
								}
							}
						}

						// закрываем файл, если открыт
						if ((filest[a] == 1) && (fp[a] != NULL)) {
							fclose(fp[a]); filest[a] = 0; fp[a] = NULL;
						}

						// FILE COPY
						// Копируем дополнительный файл
						if (strlen(path2)!=0){
							snprintf(scmd,256,"cp -f %s %s", path, path2); //t_path
							system(scmd);
						}

						// Перемещаем/переименовываеем временный файл
						//snprintf(scmd,256,"mv -f %s %s", t_path, path);
						//system(scmd);


						// transfer speed
						time(&ctime);
						dif = difftime(ctime, dwnl_time);
						KB = f_size/1024;
						KBpS = (f_size / dif)/1024;
						if (dif > 0){
							sprintf(logbuffer,"Received: %s, %d KB, %d KB/s, '%s'", clientIP[a], KB, KBpS, path); log_print(logbuffer);
						}else{
							sprintf(logbuffer,"Received: %s, %d KB, '%s'", clientIP[a], KB, path); log_print(logbuffer);
						}



						// Разрываем соединение, если так указано в настройках
						if (keep_connections == 0){
							goto labelbreak;
						}
					}

				}else{
					sprintf(logbuffer,"The file was unexpectedly closed: %s", path); log_print(logbuffer);
					write(client[a], "ERROR: FILE IS CLOSED", 21);
				}


			// GETTING HEADER, INFO & COMMANDS
			}else if ((state[a] == 0) || (state[a] == -1)) {

				// decoding header
				Decode_File(mesg,rcvd,0);
				mesg[rcvd]=0;

				//printf("header size: %d\n",rcvd);
				//printf("header: %s\n",mesg);
				dl_simple = 0;
				l = strsplit(val,mesg," ");
				if (l>0)
				{
					// Сначала ищем авторизацию
					for (i=0;i<l;i++){
						l2 = strsplit(val2,val[i].s,"=");

						if (strncmp(val2[0].s,"auth",4) == 0) {
							//printf("rcvd=%s, real=%s\n",val2[1].s,ukey);

							if (strncmp(val2[1].s,ukey,8)==0){
								// Авторизация прошла
								state[a] = 0;
								admin[a] = 0;
								break;
							/*
							}else if (strncmp(val2[1].s,akey,8)==0){
								// Авторизация АДМИНИСТРАТОРА прошла
								if (admin[a] == 0) {
									//fprintf(flog,"Admin logged\n");
									sprintf(logbuffer,"Admin logged",clientIP[a]); log_print(logbuffer);
								}
								state[a] = 0;
								admin[a] = 1;
								break;
							*/
							}else{
								write(client[a], "ERROR: AUTHORIZATION FAILED", 27);
								goto labelbreak;
							}
						}
					}

					// Если авторизованы
					ifz = 0; ipth = 0;m_time=""; encode=0;
					if (state[a] == 0){
						for (i=0;i<l;i++){
							l2 = strsplit(val2,val[i].s,"=");

							//printf("cmd: %s , %s\n",val2[0].s,val2[1].s);

							if (strncmp(val2[0].s,"path",4) == 0) {
								if (strncmp(storage,val2[1].s,strlen(storage))==0) {
									f_path = val2[1].s;
									printf("%s\n", f_path);
									if (strstr(f_path, "stream") != 0) {
										memcpy(cam_MAC,f_path+12,12);
										memcpy(cam_NUMs,f_path+32,1);
									}else if (strstr(f_path, "by-date") != 0) {
										memcpy(cam_MAC,f_path+12,12);
										memcpy(cam_NUMs,f_path+44,1);
									}
									cam_MAC[12]=0;
									cam_NUMs[1]=0;
									//printf("mac=%s num=%s\n",cam_MAC, cam_NUMs);
									cam_NUM = atoi(cam_NUMs);
									ipth = 1;
								}else{
									f_path = "";
									write(client[a], "ERROR: PATH ROOT", 16);
								}

							}else if (strncmp(val2[0].s,"secpath",7) == 0) {
								// Second path
								if (strncmp(storage,val2[1].s,strlen(storage))==0) {
									strcpy(path2,val2[1].s);
									//printf("secondary file: %s\n",path2);
								}else{
									path2[0]=0;
									//snprintf(path2,256,"");
								}


							}else if (strncmp(val2[0].s,"auth",4) == 0) {
								// Should be already authorized

							}else if (strncmp(val2[0].s,"file_size",9) == 0) {
								f_size = atoi(val2[1].s);
								ifz = 1;
								file_buff = malloc(f_size);

							}else if (strncmp(val2[0].s,"mod_time",8) == 0) {
								m_time = val2[1].s;

							}else if (strncmp(val2[0].s,"encode",6) == 0) {
								encode = atoi(val2[1].s);
								//printf("Encoding algorithm %d",encode);

							//}else if (strncmp(val2[0].s,"server",6) == 0) {
							//	if (admin[a] == 1) ExecCommand(a, val2[1].s);


							}else if (strncmp(val2[0].s,"buffsize",8) == 0) {
								dl_buffsize = atoi(val2[1].s);
								dl_simple = 1;

							}else if (strncmp(val2[0].s,"offset",6) == 0) {
								dl_offset = atoi(val2[1].s);
								dl_simple = 1;

							}else if (strncmp(val2[0].s,"getfile",7) == 0) {
								if (allow_download == 1) {

									//sprintf(logbuffer,"File request %s '%s'",clientIP[a],val2[1].s); log_print(logbuffer);
									//snprintf(path,256,"%s%s",storage,f_path);

									f_path = val2[1].s;
									//strcpy(f_path,val2[1].s);

									snprintf(path,256,"%s%s",storage,f_path);
									//strcpy(path,f_path);

									//printf("fl: %s\n" ,path);
									fp[a] = fopen ( path, "rb" );
									if (fp[a] != NULL) {
										filest[a] = 1;

										if (fseeko(fp[a], 0 , SEEK_END) != 0) {/* Handle error */}
										f_size = ftello(fp[a]);
										if (f_size == -1) {
											/* Handle error */
											write(client[a], "ERROR: FILE SIZE ERROR", 22);
											state[a] = 0;
											filest[a] = 0;
										}else{
											if (dl_simple == 1){
												filest[a] = 0;
												state[a] = 0;
												if  (dl_offset >  f_size-1) {
													write(client[a], "DONE", 4);
												}else{
													buff_len = dl_buffsize;
													if ((dl_offset + dl_buffsize) >  f_size) {
														buff_len = f_size - dl_offset;
													}else{
													}
													fseek(fp[a],dl_offset,SEEK_SET);
													fread (fbuff, sizeof(char), buff_len, fp[a]);
													Decode_File(fbuff,buff_len,0);
													write(client[a], fbuff, buff_len);
												}

											}else{
												snprintf(scmd,256,"file_size=%d",f_size);
												write(client[a], scmd, strlen(scmd));
												state[a] = 2;
												time(&last_time[a]);
											}
										}



									}else{
										write(client[a], "ERROR: FILE DOESNT EXIST", 24);
									}
								}else{
									write(client[a], "ERROR: NOT ALLOWED", 18);
								}

							}else{
								//fprintf(flog,"unknown command: '%s'\n",val[i].s);
								sprintf(logbuffer,"unknown command: '%s'",val[i].s); log_print(logbuffer);
							}
						}
					}
				}

				// PREPARING TO RECEIVE / Подготовка файла к получению
				if (state[a] == -1){
					// Проверка, авторизовался ли
					write(client[a], "ERROR: AUTHORIZE FIRST", 22);
				}else{
					// Если есть путь и размер
					if ((ipth == 1) && (ifz == 1)) {
						//snprintf(path,256,"%s%s",storage,f_path);

						strcpy(path, f_path);
						//sprintf(t_path,"%s.tmp0",path);

						// создаем папку
						if (make_dir == 1)  {
							makeDir(getDir(path));
							if (strlen(path2)>0){
								makeDir(getDir(path2));
							}
						}

						first_rcv = 1;


						// открываем/создаем файл
						fp[a] = fopen (path, "wb" ); //t_path, "wb" );
						if (fp[a] != NULL) {
							filest[a] = 1;
							state[a] = 1;
							seekpos = 0;
							buffpos = 0;
							blocki = 0;

							// сохраняем имя файла
							fname[a] = malloc(strlen(path)+1);//t_path
							strcpy(fname[a], path);//t_path

							//printf("ready to recieve\n");

							write(client[a], "READY", 5);

							time(&dwnl_time);
							time(&last_time[a]);

						}else{
							write(client[a], "ERROR: WRONG FILENAME", 21);
							filest[a] = 0;
						}


					}
				}



			}else if ((state[a] == 2)){
				// decoding header
				Decode_File(mesg,rcvd,0);
				mesg[rcvd]=0;

				// SENDING FILE
				if (strncmp(mesg,"READY",5) == 0) {
					//printf("READY TO RECEIVE\n");
					// send file
					state[a] = 3;
					time(&dwnl_time);


					int nbytes_total=0, nbytes_last;
					int file_pos=0;

					while (file_pos<f_size) {
						buff_len = f_size - file_pos;
						if (buff_len > out_buffer_size) buff_len = out_buffer_size;

						// File buffer
						fseek(fp[a],file_pos,SEEK_SET);
						fread (fbuff, sizeof(char), buff_len, fp[a]);

						// Encoding
						switch (encode)
						{
						case 0:
							break;
						case 1:
							Decode_File(fbuff,buff_len,file_pos);
							break;
						}

						// Writing to socket
						nbytes_total = 0;
						while (nbytes_total < buff_len) {
							nbytes_last = write(client[a], fbuff + nbytes_total, buff_len - nbytes_total);
							if (nbytes_last == -1) {
								if (errno == EBADF) {
									sprintf(logbuffer,"Bad file number: %d",client[a]); log_print(logbuffer);
								}
								if ((filest[a] == 1) && (fp[a] != NULL)) {fclose ( fp[a] ); filest[a] = 0;}
								goto labelbreak;
							}else{
								time(&last_time[a]);
								nbytes_total += nbytes_last;
							}
						}
						file_pos += out_buffer_size;
					}

					//if (fbuff != NULL) free(fbuff);

					if ((filest[a] == 1) && (fp[a] != NULL)) {fclose ( fp[a] ); filest[a] = 0;}



				}else if (strncmp(mesg,"BREAK",5) == 0) {
					state[a] = 0;
					if ((filest[a] == 1) && (fp[a] != NULL)) {fclose ( fp[a] ); filest[a] = 0;}

				}else{
					// UNKNOWN MESSAGE, Кикаем клиента
					sprintf(logbuffer,"UNKNOWN DISCONNECT"); log_print(logbuffer);
					goto labelbreak;
				}

			}else if ((state[a] == 3)){
				// decoding header
				Decode_File(mesg,rcvd,0);

				// COMPLETE SENDING
				if (strncmp(mesg,"DONE",4) == 0) {

					state[a] = 0;
					if ((filest[a] == 1) && (fp[a] != NULL)) {fclose ( fp[a] ); filest[a] = 0;}

					// transfer speed
					time(&ctime);
					dif = difftime(ctime, dwnl_time);
					KB = f_size/1024;
					KBpS = (f_size / dif)/1024;
					if (dif > 0){
						sprintf(logbuffer,"File sent: %s, %d KB, %d KB/s, '%s'", clientIP[a], KB, KBpS, path); log_print(logbuffer);
					}else{
						sprintf(logbuffer,"File sent: %s, %d KB, '%s'", clientIP[a], KB, path); log_print(logbuffer);
					}

				}

			}

			setsockopt(srv_socket, IPPROTO_TCP, TCP_QUICKACK, &(int){1},sizeof(int));
			//break;
		}
	}

labelbreak:;
	if (file_buff!=0) free(file_buff);
	if (fbuff != NULL) free(fbuff);
    if (client[a] != -1) {
		shutdown (client[a], SHUT_RDWR);
		close(client[a]);
	}
    client[a]=-1;
    pthread_exit(NULL);
}




// SERVER START
int startServer(){

	struct sockaddr_in address;

	// Creating socket
	//if ((srv_socket = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
	if ((srv_socket = socket(2, 1, 6)) > 0) {
	}else{
		//printf("Cannot create socket.\n");
		return 1;
    }

	// Set TPC and Address
	address.sin_family = AF_INET;
	address.sin_port = htons(srv_port);
	if (strlen(srv_host) == 0) {
		srv_host = "localhost";
		address.sin_addr.s_addr = INADDR_ANY;
	}else{
		inet_pton(AF_INET, srv_host, &(address.sin_addr));
	}

	// REUSE ADDR
	setsockopt(srv_socket,SOL_SOCKET,SO_REUSEADDR, &(int){1},sizeof(int));
	//setsockopt(srv_socket, IPPROTO_TCP, TCP_NODELAY, &(int){1},sizeof(int));
	setsockopt(srv_socket, SOL_SOCKET, SO_RCVBUF,&(int){buffer_size*2},sizeof(int));
	setsockopt(srv_socket, SOL_SOCKET, SO_SNDBUF,&(int){out_buffer_size*2},sizeof(int));

	// BINDING
    if (bind(srv_socket, (struct sockaddr *) &address, sizeof(address)) == 0){
	}else{
		//fprintf(flog,"Unable bind: Address already in use. Try Later.\n");
		sprintf(logbuffer,"Unable bind: Address already in use. Try Later."); log_print(logbuffer);
		return 1;
    }

	// START LISTENING
	if (listen(srv_socket, 999) == 0) {
	}else{
		//printf("Listen Error\n");
		return 1;
	}

	// CLEAR CLIENT's CONNECTIONS
	int i;
	for (i=0; i<CONNMAX; i++) {
        client[i]=-1;
	}

	return 0;
}



 //ACCEPTING
int accepting() {
	struct sockaddr_in address;
	socklen_t addrlen;
	int ac;
	int i=0;
	int addr;
	char *ip;


	addrlen = sizeof(address);
	ac =  accept (srv_socket, (struct sockaddr *) &address, &addrlen);
	// inet_ntoa
	if (ac == -1) {
		//printf("accept error\n");
		return 1;
	}else if (ac == 0) {
		return 1;
	}else{
		// ACCEPTED
		//addr= inet_ntoa(address.sin_addr);

		ip = (char*)inet_ntoa(address.sin_addr);
		//ip = (char*)addr;
		//printf ("ip: %s\n",ip);
	}


	time_t ctime;
	time(&ctime);


	// FREE SLOT
	int status;
	for (i=0;i<CONNMAX;i++)
	{
		if (client[i] == -1) {
			// Настройки клиента
			client[i] = ac;
			if (clientIP[i] != NULL) free(clientIP[i]);

			clientIP[i] = malloc(strlen(ip)+1);
			strcpy(clientIP[i],ip );

			if (log_events==1) {sprintf(logbuffer,"Accepted: %s  at %d",clientIP[i],i); log_print(logbuffer);}

			last_time[i] = ctime;
			state[i]=-1;
			admin[i]=0;

			// Запускаем поток для клиента
			status = pthread_create(&sck_thrd[i], NULL, ClientWork, (void *) (intptr_t)i);
			if ( status != 0 ) {
				sprintf(logbuffer,"Cannot create thread"); log_print(logbuffer);
			}else{
				pthread_detach(sck_thrd[i]);
			}

			return 0;
		}
	}

	if (log_events==1) {sprintf(logbuffer,"Cannot find free slot for %s",ip); log_print(logbuffer);}
	close(ac);
	return 1;
}

                           // CHECK STATE
void *CheckState() {
	time_t ctime;
	int i;
	float dif;
	char sst[256];

	while(1){
		usleep(100000);
		time(&ctime);

		for (i=0;i<CONNMAX;i++)
		{
			if (client[i] != -1) {
				dif = difftime(ctime, last_time[i]);

				// Проверка таймауута на авторизацию
				if (authorized_timeout == 1) {
					if (state[i] == -1) {
						if (dif>max_timeout) {
							// Кикаем с уведомлением
							printf("kick 1\n");
							write(client[i], "AUTHORIZATION TIMEOUT", 21);
							shutdown (client[i], SHUT_RDWR);
							close(client[i]);
							client[i]=-1;
						}
					}
				}

				// Проверка таймауута на копирование файла
				if (state[i] == 1) {
					if (dif>max_timeout) {

						// Закрываем файл
						if ((fp[i] != NULL) && (filest[i] == 1)) {
							fclose (fp[i]);
							filest[i] = 0;
							// Удаляем недокаченный файл
							if (remove_broken = 1) {
								snprintf(sst,256,"rm -f %s",fname[i]);
								system(sst);
							}
						}

						// Кикаем клиента с уведомлением
						printf("kick 2\n");
						write(client[i], "TIMEOUT", 7);
						shutdown (client[i], SHUT_RDWR);
						close(client[i]);
						client[i]=-1;

					}
				}

				// Кикаем, если не ответил на запрос о скачке файла
				if (state[i] == 3) {
					if (dif>max_timeout) {
						// Кикаем клиента с уведомлением
						if ((fp[i] != NULL) && (filest[i] == 1)) { fclose (fp[i]); filest[i] = 0;}
						printf("kick 3\n");
						write(client[i], "TIMEOUT", 7);
						shutdown (client[i], SHUT_RDWR);
						close(client[i]);
						client[i]=-1;
					}
				}


				// Разрываем соединение, если ничего не происходит
				if (keep_connections == 0){
					if (state[i] == 0) {
						if (dif>max_timeout) {

							// Кикаем с уведомлением
							printf("kick 4\n");
							write(client[i], "TIMEOUT", 7);
							shutdown (client[i], SHUT_RDWR);
							close(client[i]);
							client[i]=-1;
						}
					}
				}


			}
		}

	}
}
void *CheckTime() {
	time_t now;
	struct tm *now_tm;
	int yeah, hour, min, sec;
	while(1){
		usleep(100000);
		now = time(NULL);
		now_tm = localtime(&now);
		sec = now_tm->tm_sec;
		min = now_tm->tm_min;
		hour = now_tm->tm_hour;
		//yeah = now_tm->tm_year+1900;
		if (min==0 && sec==0) {
			exit(1);
		}
	}
}


// MAIN
int main() {
	int retv;
	printf("starting\n");

	// Load Config
	retv = load_config();
	if (retv==1) {
		sprintf(logbuffer,"Config file error"); log_print(logbuffer);
	}else{
		sprintf(logbuffer,"Configured"); log_print(logbuffer);
	}

	//Start server
	if (startServer() != 0) {
		sprintf(logbuffer,"Cannot start server at %s:%d",srv_host,srv_port); log_print(logbuffer);
		printf("%s\n", logbuffer);
		exit(1);
	}

	sprintf(logbuffer,"Started at %s:%d",srv_host,srv_port); log_print(logbuffer);
	int ret, i;

	// Checking client's status
	pthread_t cs, cs1;
	ret = pthread_create(&cs, NULL, CheckState, 0);
	//ret = pthread_create(&cs1, NULL, CheckTime, 0);

	while(1){
		usleep(10000);
		ret = accepting();

	}
}


