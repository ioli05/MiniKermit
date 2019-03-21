#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000
#define INITVAL 0
#define MAXLENGTH 257
#define TIMEOUT 5


int main(int argc, char** argv) {
	msg t, aux;
	msg *y;
	init(HOST, PORT); //stabilire conexiune

	frame frame;
	initialData date;
	unsigned char crc;
	int nrTries = 0;

	//Crearea pachetului Send-Init initializand campurile caracteristice
	frame.SOH = 0x01;
	frame.LEN = MAXLENGTH - 2;
	frame.SEQ = INITVAL;
	frame.TYPE = 'S';

	//lungimea campului data este 250 (dimensiunea kermitului - 7)
	date.MAXL = MAXLENGTH - 7;
	date.TIME = TIMEOUT;
	date.NPAD = 0x00;
	date.PADC = 0x00;
	date.EOL = 0x0D;
	date.QCTL = date.QBIN = date.CHKT = date.REPT = date.CAPA = date.R = 0x00;

	//Copiez in campul data al mini-kermitului, structura speficifica pachetului send-init
	memset(&frame.DATA, 0, date.MAXL);
	memcpy(frame.DATA, &date, 11);

	//Folosesc o variabila auxiliara pentru a salva campurile necesare si pentru a calcula crc-ul
	aux.len = MAXLENGTH - 3; //Lungimea fara campul check si mark din frame
	memcpy(aux.payload, &frame, aux.len);

	crc = crc16_ccitt(aux.payload, aux.len);

	//Initializarea si ultimelor doua campuri din frame, cu crc-ul calculat deja
	frame.CHECK = crc;
	frame.MARK = date.EOL;

	t.len = MAXLENGTH;
	memcpy(t.payload, &frame, MAXLENGTH);

	printf("[%s]Trimit pachetul de SEND-INIT cu secventa numarul:%hhu\n", argv[0], frame.SEQ);

	send_message(&t);

	//Setez un numar de trei incercari de a retrimite mesajul fie ca este timeout fie ca este NACK
	while (nrTries < 4) {
		y = receive_message_timeout(5000);
		if (y == NULL) { printf("Caz de TIMEOUT pentru SEND-INIT.\n"); return 0; }
		if (y != NULL && y->payload[3] == 'Y') {
			printf("[%s]Primit pachetul ACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
			break;
		}
		if (y != NULL && y->payload[3] == 'N') {
			printf("[%s] Primit pachetul NACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
			t.payload[2] = (y->payload[2] + 1) % 64;
			printf("[%s] Retrimit pachetul cu secventa numarul:%d.\n", argv[0], t.payload[2]);
			send_message(&t);
		}
		nrTries++;
	}
	//Daca dupa 3 retrimiteri nu avem un pachet de tip ACK opresc transmisia
	if (y == NULL || y->payload[3] == 'N') {
		printf("[%s] Iesire fortata pachet FILE-HEADER\n", argv[0]);
		return 0;
	}

	//Daca nu este null atunci putem trimite urmatorul pachet
	for (int k = 1; k < 4; k++) {
		//Initializam campurile pentru pachetul de tip File Header
		memset(&frame, 0, MAXLENGTH);
		frame.SOH = 0x01;
		frame.LEN = MAXLENGTH - 2;
		frame.SEQ = (y->payload[2] + 1) % 64;
		frame.TYPE = 'F';

		//Initializez campul DATA si copiez numele fisierului
		memcpy(frame.DATA, argv[k], strlen(argv[k]));

		//Variabila auxiliara, pentru a selecta campurile necesare crc-ului 
		aux.len = 254;
		memcpy(aux.payload, &frame, aux.len);

		//Folosesc o variabila auxiliara pentru calcularea crc-ului
		crc = crc16_ccitt(aux.payload, MAXLENGTH - 3);

		//Initializarea campului check cu crc-ul corespunzator
		frame.CHECK = crc;
		frame.MARK = 0x0D;

		//Initializez campurile mesajului ce urmeaza a fi trimis
		t.len = 257;
		memcpy(t.payload, &frame, t.len);

		printf("[%s]Trimit pachetul de FILE-HEADER cu secventa numarul:%hhu\n", argv[0], frame.SEQ);
		send_message(&t);

		while (nrTries < 4) {
			y = receive_message_timeout(5000);
			if (y == NULL) { printf("Caz de TIMEOUT pentru FILE HEADER.\n"); send_message(&t); }
			if (y != NULL && y->payload[3] == 'Y') {
				printf("[%s]Primit pachetul ACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
				break;
			}
			if (y != NULL && y->payload[3] == 'N') {
				printf("[%s] Primit pachetul NACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
				t.payload[2] = (y->payload[2] + 1) % 64;
				printf("[%s] Retrimit pachetul cu secventa numarul:%d.\n", argv[0], t.payload[2]);
				send_message(&t);
			}
			nrTries++;
		}
		//Daca dupa 3 retrimiteri nu am primit un pachet de tip ACK atunci inchid transmisia
		if (y == NULL || y->payload[3] == 'N') {
			printf("[%s] Iesire fortata pachet FILE-HEADER\n", argv[0]);
			return 0;
		}

		//Deschid fisierul pentru citirea datelor
		FILE *file = fopen(argv[k], "r");

		while (!feof(file)) {

			memset(&frame, 0, 257);
			//Citesc din fisier
			int nrBytes = fread(frame.DATA, 1, MAXLENGTH - 7, file);

			//Daca dimensiunea lui nrBytes este mai mica decat dimensiunea maxima a campului data (250)
				//atunci campul LEN va avea valoarea campului data 
			if (nrBytes != 250) {
				frame.LEN = nrBytes + 5;
			}
			else {
				frame.LEN = MAXLENGTH - 2;
			}
			//Initializarea primelor 4 campuri din MINI-KERMIT
			frame.SOH = 0x01;
			frame.SEQ = INITVAL;
			frame.TYPE = 'D';

			//Incrementez numarul secventei deoarece am primit deja un pachet de tip ACK
			frame.SEQ = (y->payload[2] + 1) % 64;

			//Variabila auxiliara, pentru a selecta campurile necesare crc-ului 
			aux.len = 254;
			memcpy(aux.payload, &frame, aux.len);

			//Caclularea crc-ului corezpunzator
			crc = crc16_ccitt(aux.payload, aux.len);

			//Initializarea campurilor check si mark
			frame.CHECK = crc;
			//printf("[SENDER]:%d\n", frame.CHECK);
			frame.MARK = 0x0D;
			//Initializarea campurilor mesajului
			t.len = MAXLENGTH;
			memcpy(t.payload, &frame, t.len);

			printf("[%s]Trimit pachetul de DATE cu secventa numarul:%hhu\n", argv[0], frame.SEQ);
			send_message(&t);

			//Incerc retrimiterea de 3 ori a pachetelor pentru a fi primite corect
			while (nrTries < 3) {
				y = receive_message_timeout(5000);
				if (y == NULL) { printf("Caz de TIMEOUT pentru DATA.\n"); send_message(&t); }
				if (y != NULL && y->payload[3] == 'Y') {
					printf("[%s]Primit pachetul ACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
					break;
				}
				if (y != NULL && y->payload[3] == 'N') {
					printf("[%s] Primit pachetul NACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
					t.payload[2] = (y->payload[2] + 1) % 64;
					printf("[%s] Retrimit pachetul cu secventa numarul:%d.\n", argv[0], t.payload[2]);
					send_message(&t);
				}
				nrTries++;
			}
			//Daca dupa 3 retrimiteri nu am primit pachet de tip ACK, inchei transmisia
			if (y == NULL || y->payload[3] == 'N') {
				printf("[%s] Iesire fortata pachet DATE.\n", argv[0]);
				return 0;
			}
		}
		fclose(file);
		printf("Iesit din while\n");

		//Am ajuns la finalul fisierului si trimitem pachetul de final de fisier

		//Initializez campurile pachetului de tip End of File
		memset(&frame, 0, 257);
		frame.SOH = 0x01;
		frame.LEN = MAXLENGTH - 2;
		frame.SEQ = (y->payload[2] + 1) % 64;
		frame.TYPE = 'Z';
		memset(frame.DATA, 0, 250);

		aux.len = 254;
		memcpy(aux.payload, &frame, aux.len);

		//Folosesc o variabila auxiliara pentru calcularea crc-ului
		crc = crc16_ccitt(aux.payload, MAXLENGTH - 3);

		//Initializarea campului check cu crc-ul corespunzator
		frame.CHECK = crc;
		frame.MARK = 0x0D;

		t.len = MAXLENGTH;
		memcpy(t.payload, &frame, t.len);

		printf("[%s]Trimit pachetul de End of File cu secventa numarul:%hhu\n", argv[0], frame.SEQ);

		send_message(&t);
		//Incerc retrimiterea de 3 ori a pachetelor pentru a fi primite corect
		while (nrTries < 3) {
			y = receive_message_timeout(5000);
			if (y == NULL) { printf("Caz de TIMEOUT pentru End of File.\n"); send_message(&t); }
			if (y != NULL && y->payload[3] == 'Y') {
				printf("[%s]Primit pachetul ACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
				break;
			}
			if (y != NULL && y->payload[3] == 'N') {
				printf("[%s] Primit pachetul NACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
				t.payload[2] = (y->payload[2] + 1) % 64;
				printf("[%s] Retrimit pachetul cu secventa numarul:%d.\n", argv[0], t.payload[2]);
				send_message(&t);
			}
			nrTries++;
		}
		//Daca dupa 3 retrimiteri nu am primit pachet de tip ACK, inchei transmisia
		if (y == NULL || y->payload[3] == 'N') {
			printf("[%s] Iesire fortata pachet End of File.\n", argv[0]);
			return 0;
		}
	}

	//Initializez campurile pachetului de tip End of Text
	memset(&frame, 0, 257);
	frame.SOH = 0x01;
	frame.LEN = MAXLENGTH - 2;
	frame.SEQ = (y->payload[2] + 1) % 64;
	frame.TYPE = 'B';
	memset(frame.DATA, 0, 250);

	aux.len = 254;
	memcpy(aux.payload, &frame, aux.len);

	//Folosesc o variabila auxiliara pentru calcularea crc-ului
	crc = crc16_ccitt(aux.payload, MAXLENGTH - 3);

	//Initializarea campului check cu crc-ul corespunzator
	frame.CHECK = crc;
	frame.MARK = 0x0D;

	t.len = MAXLENGTH;
	memcpy(t.payload, &frame, t.len);

	printf("[%s]Trimit pachetul de End of Text cu secventa numarul:%hhu\n", argv[0], frame.SEQ);

	send_message(&t);
	//Incerc retrimiterea de 3 ori a pachetelor pentru a fi primite corect
	while (nrTries < 3) {
		y = receive_message_timeout(5000);
		if (y == NULL) { printf("Caz de TIMEOUT pentru End of Text.\n"); send_message(&t); }
		if (y != NULL && y->payload[3] == 'Y') {
			printf("[%s]Primit pachetul ACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
			break;
		}
		if (y != NULL && y->payload[3] == 'N') {
			printf("[%s] Primit pachetul NACK cu secventa numarul:%hhu\n", argv[0], y->payload[2]);
			t.payload[2] = (y->payload[2] + 1) % 64;
			printf("[%s] Retrimit pachetul cu secventa numarul:%d.\n", argv[0], t.payload[2]);
			send_message(&t);
		}
		nrTries++;
	}
	//Daca dupa 3 retrimiteri nu am primit pachet de tip ACK, inchei transmisia
	if (y == NULL || y->payload[3] == 'N') {
		printf("[%s] Iesire fortata pachet End of Text.\n", argv[0]);
		return 0;
	}
	
    return 0;
}
