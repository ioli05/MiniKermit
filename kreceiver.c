#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001
#define NACK 2
#define MAXLENGTH 257
#define TIMEOUT 5

int main(int argc, char** argv) {
	msg t, aux;
	msg *y;
	init(HOST, PORT); //stabilire conexiune
	frame frame;
	initialData date;
	int recievedSendInit = 0;
	unsigned char crc;
	char name[260];
	int lastSeq = -22222;
	FILE* file;
	int nrNull = 0;

	while (1) {
		//Primesc mesajul de la sender in variabila y;
		y = receive_message_timeout(5000);
		//Daca nu am primit niciun mesaj, intram pe cazul de Time Out
		if (y == NULL) {
			//Daca este pentru pachetul Send-Init, mai astept inca 10s altfel inchei transmisia
			if (recievedSendInit == 0) {
				while (y == NULL && nrNull != 2) {
					y = receive_message_timeout(5000);
				}
				if (y == NULL) {
					printf("Iesire din reciever caz TIMEOUT!\n");
					return 0;
				}
			}
			if (recievedSendInit == 1) {
				//Vom mai astepta primirea unui mesaj timp de 3 ori. In caz contrar intrerupem
				//conexiunea;
				while (y == NULL && nrNull != 3) {
					printf("[%s]Caz de TIMEOUT pachet.\n", argv[0]);
					memcpy(t.payload, &frame, MAXLENGTH);
					t.len = MAXLENGTH;
					nrNull++;
					send_message(&t);
					y = receive_message_timeout(5000);
				}
				if (y == NULL) {
					printf("Iesire fortata.\n");
					return 0;
				}
			}
		}

		nrNull = 0;

		if (y != NULL){
			//Daca am primit un mesaj, atunci ii recalculam crc-ul
			memset(&frame, 0, MAXLENGTH);
			memcpy(&frame, y->payload, y->len);

			aux.len = MAXLENGTH - 3;
			memcpy(aux.payload, &frame, aux.len);

			crc = crc16_ccitt(aux.payload, aux.len);
			//Daca sunt egale atunci am primit corect pachetul
			if (crc == frame.CHECK) {
				//Verific daca am mai primit sau nu pachetul cu aceasta secventa
				if (lastSeq != frame.SEQ) {
					lastSeq = frame.SEQ;
				}
				//Daca am mai primit secventa atunci retrimit un pachet de tip ACK
				else {
					frame.TYPE = 'Y';
					frame.SEQ = (frame.SEQ + 1) % 64;

					memcpy(t.payload, &frame, MAXLENGTH);
					t.len = MAXLENGTH;

					printf("[%s] Trimit pachetul de ACK cu secventa numarul:%hhu\n", argv[0], frame.SEQ);

					send_message(&t);
					continue;
				}
				//Trimitem in cazul in care pachetul este de tip Send-Init ack cu setarile emitorului
				if (frame.TYPE == 'S') {
					//lungimea campului data este 250 (dimensimunea kermitului - 7)
					date.MAXL = MAXLENGTH - 7;
					date.TIME = TIMEOUT;
					date.NPAD = 0x00;
					date.PADC = 0x00;
					date.EOL = 0x0D;
					date.QCTL = date.QBIN = date.CHKT = date.REPT = date.CAPA = date.R = 0x00;

					//Copiez in campul data al mini-kermitului, structura speficifica pachetului send-init
					memset(&frame.DATA, 0, date.MAXL);
					memcpy(frame.DATA, &date, 11);

				}
				frame.TYPE = 'Y';
				frame.SEQ = (frame.SEQ + 1) % 64;

				memcpy(t.payload, &frame, MAXLENGTH);
				t.len = MAXLENGTH;

				printf("[%s] Trimit pachetul de ACK cu secventa numarul:%hhu\n", argv[0], frame.SEQ);

				send_message(&t);

				//Daca pachetul este de tip Send-Init cu ACK atunci initializam variabila 
					//recievedSendInit
				if (y->payload[3] == 'S') {
					recievedSendInit = 1;
				}
				//Daca pachetul este de tip File-Header atunci deschid fisierul pentru scriere
				if (y->payload[3] == 'F') {
					memset(&frame, 0, MAXLENGTH);
					memcpy(&frame, y->payload, y->len);

					memset(name, 0, 250);
					strcpy(name, "recv_");
					strcat(name, frame.DATA);
					file = fopen(name, "w");
					printf("[%s] Numele fisierului: %s.\n", argv[0], name);
				}

				//Scriu in fisier secventa primita dar daca nu a mai fost primita inca o data
				if (y->payload[3] == 'D' ){
					memset(&frame, 0, MAXLENGTH);
					memcpy(&frame, y->payload, y->len);
					fwrite(frame.DATA, 1, frame.LEN - 5, file);
				}
				//Daca pachetul este de tip End of File, inchid fisierul
				if (y->payload[3] == 'Z') {
					fclose(file);
				}
				//Daca pachetul este de tip End of Text, inchid transmisia
				if (y->payload[3] == 'B') {
					return 0;
				}
			}
			//Daca crc-ul este diferit, atunci trimit un pachet de tip NACK
			else {
			
				frame.TYPE = 'N';
				frame.SEQ = (frame.SEQ + 1) % 64;
				
				memcpy(t.payload, &frame, MAXLENGTH);
				t.len = MAXLENGTH;
			
				printf("[%s] Trimit pachetul de NACK cu secventa numarul:%hhu\n", argv[0], frame.SEQ);
			
				send_message(&t);
			}
		}
	}
	return 0;
}
