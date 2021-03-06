// Test of cheap 13.56 Mhz RFID-RC522 module from eBay
// This code is based on Martin Olejar's and Thomas Kirchner's MFRC522 libraries. Minimal changes
// Adapted for FRDM-K64F from Freescale, in 07/21/2014 by Clovis Fritzen.

#include "mbed.h"
#include "MFRC522.h"
#include "EthernetInterface.h"

#include "mbed_config.h"
#include <string.h>
#include <stdio.h>
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/version.h"
#include "mbedtls/aes.h"

#define ENABLE_ECDSA



// FRDM-K64F (Freescale) Pin for MFRC522 reset
#define MF_RESET PTD0

// Serial connection to PC for output*/
static BufferedSerial pc(PTC17, PTC16); // serial comm on the FRDM board

// MFRC522    RfChip   (SPI_MOSI, SPI_MISO, SPI_SCK, SPI_CS, MF_RESET);
MFRC522 RfChip(PTD2, PTD3, PTD1, PTE25, PTD0);

EthernetInterface net;
DigitalOut LedGreen(LED2);
DigitalOut LedBlue(LED3);
DigitalOut LedRed(LED1);

struct message
{ // Structure declaration
  char type;
  int length;
  char *payload;
};

void turnOnBlueLight()
{
  LedGreen = 1; // off
  LedBlue = 0;  // on
  LedRed = 1;   // off
}
void turnOnRedLight()
{
  LedGreen = 1; // off
  LedBlue = 1;  // off
  LedRed = 0;   // on
}

void turnOnGreenLight()
{
  LedGreen = 0; // on
  LedBlue = 1;  // off
  LedRed = 1;   // off
}

void turnOffAllLights()
{
  LedGreen = 1; // off
  LedBlue = 1;  // off
  LedRed = 1;   // off
}

void findUniqueDeviceId(char temp[])
{

  // 4 registers with 32 bit- 4 bytes each and all together gives 128 bit unique id of the device- see manual page 305
  int IdentificationRegistersAddresses[] = {0x40048054, 0x40048058, 0x4004805C, 0x40048060};
  int i;
  for (i = 0; i < 4; i++)
  {
    char *pointer = (char *)IdentificationRegistersAddresses[i];

    temp[i * 4] = pointer[0];
    temp[(i * 4) + 1] = pointer[1];
    temp[(i * 4) + 2] = pointer[2];
    temp[(i * 4) + 3] = pointer[3];
  }
  /*
    for (int i = 0; i < 16; ++i) {
        printf("%x", temp[i]);
    }
  printf("\r\n");*/
}

void print_id(char *array, int n)
{
  printf("%.*s ", 1, array);
  for (int i = 1; i < n; i++)
  {
    printf("%X ", array[i]);
  }
  printf("\r\n");
}

void print_array(unsigned char *array, int n)
{

  for (int i = 0; i < n; i++)
  {
    printf("%X ", array[i]);
  }
  printf("\r\n");
}

void print_byte_array(char * array, int n){
   for (int i = 0; i < n; i++)
  {
    printf("%X ", array[i]);
  }
  printf("\r\n");
}

int sendMessageToServer(TCPSocket *socket, struct message msg)
{
  char dev_type[] = "#";
  char card_type[] = "@";
  char hello_msg_type[] = "!";
  char encrypted_aes_type[]="^";
  // Send the unique id of the RFID card to server
  int sent_bytes = (*socket).send(msg.payload, msg.length);
  printf("sent bytes are: %d \n", sent_bytes);
  if (msg.type == dev_type[0])
  {
    printf("Device UID: ");
    print_id(msg.payload, msg.length);
  }
  else if (msg.type == card_type[0])
  {
    printf("Card UID: ");
    print_id(msg.payload, msg.length);
  }
  else if (msg.type == hello_msg_type[0])
  {
    printf("client sends:%s", msg.payload);
    printf("\n\r");
  }
  else if (msg.type == encrypted_aes_type[0])
  {
    printf("encrypted aes key: ");
    print_id(msg.payload, msg.length);
    printf("\n\r");
  }
  return sent_bytes;
}

void receiveResponseFromServer(TCPSocket *socket, int bufferLength, int isCard)
{

  char allow[] = "Door is opened.";
  char do_not_allow[] = "Sorry, you can't access.";
  // Recieve a simple response and print out the response line
  char rbuffer[bufferLength];
  memset(rbuffer, 0, bufferLength); // clear the previous message
  struct message response_message;
  int rcount = (*socket).recv(rbuffer, sizeof rbuffer);

  response_message.type = rbuffer[0];
  response_message.length = (sizeof rbuffer);
  response_message.payload = rbuffer;

  printf("recv %d [%.*s]\n", rcount, strstr(response_message.payload, "\r\n") - response_message.payload, response_message.payload);

  if (isCard)
  {
    if (strcmp(allow, response_message.payload) == 0)
    {
      turnOnGreenLight();
    }
    else if (strcmp(do_not_allow, response_message.payload) == 0)
    {
      turnOnRedLight();
    }
    ThisThread::sleep_for(3s);
  }
}

int checkIfServerIsDown(int scount)
{

  if (scount <= 0)
  {

    LedBlue = 1;
    LedGreen = 0;
    LedRed = 0; // on
    printf("Server is down.Please wait!\n");
    ThisThread::sleep_for(3s);
    return 1;
  }
  return 0;
}
void aesEncryption(char plainText[], unsigned char aes_key[], char encrypt_output[], int isCard){
  mbedtls_aes_context aes;
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_enc(&aes, aes_key, 256);

  unsigned char output_buffer[16];
  mbedtls_aes_crypt_ecb( &aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)plainText, output_buffer);
  mbedtls_aes_free( &aes );
  printf("-----aes encrypted message---- \n");
  print_array(output_buffer,16);
  memmove(encrypt_output+1, output_buffer, 16); //insert type of message at buffer
  if(isCard==0){
    encrypt_output[0] = '#';
  }
  else{
    encrypt_output[0] = '@';
  }
  
  printf("-----prosthetw sima---- \n");
  print_id(encrypt_output,17);
  printf("~~~~~~~~~~\n");
  
}

void RFIDCommunication(int communication_failed, TCPSocket *sock,unsigned char aes_key[])
{

  char card_id[4] = "";
  LedBlue = 1;
  printf("Scan your tag...\n");

  // Init. RC522 Chip
  RfChip.PCD_Init();

  while (communication_failed == 0)
  {

    // Look for new cards
    if (!RfChip.PICC_IsNewCardPresent())
    {
      LedGreen = 1;
      LedRed = 1;
      LedBlue = !LedBlue;
      ThisThread::sleep_for(100);
      continue;
    }

    // Select one of the cards
    if (!RfChip.PICC_ReadCardSerial())
    {
      LedGreen = 1;
      LedRed = 1;
      LedBlue = !LedBlue;
      ThisThread::sleep_for(100);
      continue;
    }

    LedBlue = 1;

    // Print Card UID
    for (uint8_t i = 0; i < RfChip.uid.size; i++)
    {
      card_id[i] = RfChip.uid.uidByte[i];
      // printf(" %X", RfChip.uid.uidByte[i]);
    }

    ThisThread::sleep_for(100);
    printf("---card id----- \n");
    print_byte_array(card_id,4);
    char encrypted_card_id[17];
    aesEncryption(card_id,aes_key,encrypted_card_id,1);

    struct message card_message;

    card_message.type = encrypted_card_id[0];
    card_message.length = (sizeof encrypted_card_id);
    card_message.payload = encrypted_card_id;

    int scount = sendMessageToServer(sock, card_message);

    // if server is down break and try again
    communication_failed = checkIfServerIsDown(scount);
    if (communication_failed)
    {
      break;
    }

    turnOffAllLights();
    receiveResponseFromServer(sock, 25, 1);
  }
}
//
int askForPublicKey(TCPSocket *socket)
{

  struct message hello_message;
  char hello_msg[] = "!public key";

  hello_message.type = hello_msg[0];
  hello_message.length = (sizeof hello_msg);
  hello_message.payload = hello_msg;

  int rec = sendMessageToServer(socket, hello_message);
  int communication_failed = checkIfServerIsDown(rec);

  return communication_failed;
}

void generateAndEncryptAesKey(unsigned char aes_key[], char public_key[],size_t public_key_len,size_t aes_key_len,char ecryptedAesKey[])
{
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
  // unsigned char key[32];

  char *pers = "aes generate key";
  int ret;

  mbedtls_entropy_init(&entropy);

  mbedtls_ctr_drbg_init(&ctr_drbg);

  if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (unsigned char *)pers, strlen(pers))) != 0)
  {
    printf(" failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret);
  }

  if ((ret = mbedtls_ctr_drbg_random(&ctr_drbg, aes_key, 32)) != 0)
  {
    printf(" failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -ret);
  }

  printf("~~~~~~~~\n");
  printf("\n");
  printf("Aes key is: ");
  print_array(aes_key, 32);
  printf("~~~~~~~~\n");
  printf("\n");


  mbedtls_pk_context pk;

  mbedtls_pk_init(&pk);
  fflush(stdout);

    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (unsigned char *)pers, strlen(pers))) != 0)
  {
    printf(" failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret);
  }

  fflush(stdout);

  if ((ret = mbedtls_pk_parse_public_key(&pk, (unsigned char*)public_key, public_key_len )) != 0)
  {
    printf(" failed\n  ! mbedtls_pk_parse_public_key returned -0x%04x\n", -ret);
  }
  int buf_size=128;
  unsigned char buf[128];
  size_t olen = 0;

 
  printf("Generating the encrypted value \n");
  printf("\n");
  printf("~~~~~~~~\n");
  if ((ret = mbedtls_pk_encrypt(&pk, aes_key, aes_key_len,buf, &olen, sizeof(buf),mbedtls_ctr_drbg_random, &ctr_drbg)) != 0)
  {
    printf(" failed\n  ! mbedtls_pk_encrypt returned -0x%04x\n", -ret);
  }

  printf("len of buf is :%d \n", sizeof(buf));
  print_array(buf,128);
  printf("~~~~~~~~\n");
  printf("\n");

  mbedtls_pk_free( &pk );
  mbedtls_entropy_free( &entropy );
  mbedtls_ctr_drbg_free( &ctr_drbg );

  memmove(ecryptedAesKey+1, buf, 128); //insert "^" at buf
  ecryptedAesKey[0] = '^';


    

}



void receivePublicKey(TCPSocket *socket, char public_key[], int n)
{
  char rbuffer[n];
  memset(rbuffer, 0, n); // clear the previous message
  // struct message response_message;
  int rcount = (*socket).recv(rbuffer, n);
  
  printf("length of public key is: %d \n", rcount);

  memcpy(public_key, rbuffer, n);
  printf("~~~~~~~~\n");
  printf("public key is:");
  for (int i = 0; i < n; ++i) {
        printf("%X ", public_key[i]);
    }
  printf("\r\n");
  // printf("copy [%.*s]\n", strstr(public_key, "\r\n") - public_key, public_key);
}

int bringUpEthernetConnection(TCPSocket *socket)
{

  net.connect();

  // Show the network address
  SocketAddress a;
  net.get_ip_address(&a);
  printf("IP address: %s\n", a.get_ip_address() ? a.get_ip_address() : "None");

  // Open a socket on the network interface, and create a TCP connection

  (*socket).open(&net);

  net.gethostbyname("192.168.1.5", &a);
  a.set_port(8080);
  (*socket).connect(a);
  return 0;
}

int main()
{
  turnOnBlueLight();

  int communication_failed = 0;

  char device_id[16];
  findUniqueDeviceId(device_id);
  //printf("mbedtls version:%d.%d.%d.\n", MBEDTLS_VERSION_MAJOR, MBEDTLS_VERSION_MINOR, MBEDTLS_VERSION_PATCH);

  while (1)
  {

    printf("Door Lock System\n");
    printf("Connecting to server...\n");
    TCPSocket socket;
    int fail=bringUpEthernetConnection(&socket);
    if(fail){
      continue;
    }

    //------------CLIENT ASK FOR PUBLIC KEY------------
    //--

    communication_failed = askForPublicKey(&socket);

    if (communication_failed)
    {
      continue;
    }

    //----------CLIENT RECIEVE PUBLIC KEY--------------------
    int public_key_length=272;
    char public_key[public_key_length];
    size_t public_key_size = sizeof public_key / sizeof public_key[0];
    printf("size:%d \n",public_key_size);

    receivePublicKey(&socket, public_key, public_key_length);

    //print_byte_array(public_key,162);
    // printf("pub key is: [%.*s]\n", strstr(public_key, "\r\n") - public_key, public_key);

    //------------------------------------------------------

    unsigned char aes_key[32];
    size_t aes_key_size = sizeof aes_key / sizeof aes_key[0];
    char ecryptedAesKey[129];
    generateAndEncryptAesKey(aes_key, public_key,public_key_size,aes_key_size,ecryptedAesKey);
    printf("-------------------------------------- \n");
    int i;
    printf("encrypted len: %d\n", sizeof(ecryptedAesKey));
    printf("%.*s ", 1, ecryptedAesKey);
    for (int i = 1; i < 129; i++)
    {
      printf("%X ", ecryptedAesKey[i]);
    }
    printf("\r\n");

    struct message encryptedAesKey_message;

    encryptedAesKey_message.type = ecryptedAesKey[0];
    encryptedAesKey_message.length = (sizeof ecryptedAesKey);
    encryptedAesKey_message.payload =ecryptedAesKey;

    int rec = sendMessageToServer(&socket, encryptedAesKey_message);


    //----------CLIENT SEND DEVICE ID--------------------
    char encrypted_device_id[17];
    printf("uncrypted device id is: \n");
    print_byte_array(device_id,16);
    aesEncryption(device_id,aes_key,encrypted_device_id,0);
    printf("-----encrypted device id---- \n");
     printf("%.*s ", 1, encrypted_device_id);
    for (int i = 1; i < 17; i++)
    {
      printf("%X ", encrypted_device_id[i]);
    }
    printf("\r\n");

    struct message device_message;

    device_message.type = encrypted_device_id[0];
    device_message.length = (sizeof encrypted_device_id);
    device_message.payload = encrypted_device_id;

    int s = sendMessageToServer(&socket, device_message);

    communication_failed = checkIfServerIsDown(s);
    if (communication_failed)
    {
      continue;
    }

    receiveResponseFromServer(&socket, 34, 0);

    //---------------SCAN TAGS-----------------------

    RFIDCommunication(communication_failed, &socket,aes_key);

    // Close the socket to return its memory and bring down the network interface
    socket.close();

    // Bring down the ethernet interface
    net.disconnect();
  }

  printf("Done\n");
}
