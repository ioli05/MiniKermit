#ifndef LIB
#define LIB
#define MAXLENGTH 257

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef struct {

	unsigned char SOH;
	unsigned char LEN;
	unsigned char SEQ;
	unsigned char TYPE;
	unsigned char DATA[250];
	unsigned short CHECK;
	unsigned char MARK;

} frame;

typedef struct {

	unsigned char MAXL;
	unsigned char TIME;
	unsigned char NPAD;
	unsigned char PADC;
	unsigned char EOL;
	unsigned char QCTL;
	unsigned char QBIN;
	unsigned char CHKT;
	unsigned char REPT;
	unsigned char CAPA;
	unsigned char R;

} initialData;
void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

void endFrame(frame frame, int seq_no) {
	msg aux;
	frame.SOH = 0x01;
	frame.LEN = MAXLENGTH - 2;
	frame.SEQ = seq_no;
	frame.TYPE = 'B';
	memset(frame.DATA, 0, 250);

	aux.len = 254;
	memcpy(aux.payload, &frame, aux.len);

	//Folosesc o variabila auxiliara pentru calcularea crc-ului
	unsigned short crc = crc16_ccitt(aux.payload, MAXLENGTH - 3);

	//Initializarea campului check cu crc-ul corespunzator
	frame.CHECK = crc;
	frame.MARK = 0x0D;

}
#endif

