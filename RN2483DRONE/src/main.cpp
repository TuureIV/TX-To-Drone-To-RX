#include <Arduino.h>

/*
 * Author: JP Meijers
 * Date: 2016-02-07
 * Previous filename: TTN-Mapper-TTNEnschede-V1
 *
 * This program is meant to be used with an Arduino UNO or NANO, conencted to an RNxx3 radio module.
 * It will most likely also work on other compatible Arduino or Arduino compatible boards, like The Things Uno, but might need some slight modifications.
 *
 * Transmit a one byte packet via TTN. This happens as fast as possible, while still keeping to
 * the 1% duty cycle rules enforced by the RN2483's built in LoRaWAN stack. Even though this is
 * allowed by the radio regulations of the 868MHz band, the fair use policy of TTN may prohibit this.
 *
 * CHECK THE RULES BEFORE USING THIS PROGRAM!
 *
 * CHANGE ADDRESS!
 * Change the device address, network (session) key, and app (session) key to the values
 * that are registered via the TTN dashboard.
 * The appropriate line is "myLora.initABP(XXX);" or "myLora.initOTAA(XXX);"
 * When using ABP, it is advised to enable "relax frame count".
 *
 * Connect the RN2xx3 as follows:
 * RN2xx3 -- Arduino
 * Uart TX -- 10
 * Uart RX -- 11
 * Reset -- 12
 * Vcc -- 3.3V
 * Gnd -- Gnd
 *
 * If you use an Arduino with a free hardware serial port, you can replace
 * the line "rn2xx3 myLora(mySerial);"
 * with     "rn2xx3 myLora(SerialX);"
 * where the parameter is the serial port the RN2xx3 is connected to.
 * Remember that the serial port should be initialised before calling initTTN().
 * For best performance the serial port should be set to 57600 baud, which is impossible with a software serial port.
 * If you use 57600 baud, you can remove the line "myLora.autobaud();".
 *
 */
#include <rn2xx3.h>
#include <SoftwareSerial.h>

String resp;

#define TXfrequency  "868100000"
#define RXfrequency  "868500000"

bool connected = false;
bool receive_packets;
bool forward_packets;
bool receive_packet_response;
int triedConnIndex = 1;

int send_index = 0;
String packet;


String initCommands[16] = {
    "sys reset",                //Resets the board
    "radio set mod lora",       //FSK, GFSK, LoRa
    "radio set freq 868100000", //frequency
    "radio set pwr 14",         //Power mode 13.5dBm = 22,4mW
    "radio set sf sf12",        //Automatic spreading factor
    "radio set afcbw 125",      //Automatic frequency correction
    "radio set rxbw 250",       //Receiving signal bandwidth
    "radio set fdev 5000",      //Frequency deviation
    "radio set prlen 8",        //Preamble length
    "radio set crc on",         //Cyclic redundancy check
    "radio set cr 4/8",         //Coding rate
    "radio set wdt 10000",       //Watch-dog timeout time in ms
    "radio set sync 12",        //Sync word with a value 0x12
    "radio set bw 250",         //Operating bandwidth
    "sys get hweui",            //Shows the hardware EUI needed for LoRaWAN operations
    "mac pause"                 //Pauses the LoRaWAN functionality
  };

SoftwareSerial loRaserial(10, 11); // RX, TX

//create an instance of the rn2xx3 library,
//giving the software serial as port to use
rn2xx3 loRaRadio(loRaserial);

void status_led_connected(boolean ledStatus)
{
  if (ledStatus){
    digitalWrite(4, HIGH);
  }
  else {
    digitalWrite(4, LOW);
  }
}

void status_led_sending(boolean ledStatus)
{
  if (ledStatus){
    digitalWrite(3, HIGH);
  }
  else {
    digitalWrite(3, LOW);
  }
}

void status_led_receiving(boolean ledStatus)
{
  if (ledStatus){
    digitalWrite(2, LOW);
  }
  else {
    digitalWrite(2, HIGH);
  }
}


void RN2483_init(){
  Serial.println("Initializing LoRa module");
  int index = 0;
  while (index<=15){
    Serial.println("Commad: " + initCommands[index] +": " + loRaRadio.sendRawCommand(initCommands[index]));
    ++ index;
  }
  Serial.println("Radio Module initialized! ");
  status_led_receiving(false);
}

void send_msg(String data){
  Serial.println("Sending message: " + data);
  data = loRaRadio.base16encode(data);
  Serial.println(loRaRadio.sendRawCommand("radio tx " + data));
  Serial.println(loRaserial.readStringUntil('\n'));
  
}

String receive_message(){
  String tempResp = "";
  Serial.println("radio rx 0: " + loRaRadio.sendRawCommand("radio rx 0"));
  while(loRaserial.available() == 0){
    }
  tempResp = loRaserial.readStringUntil('\n');
  tempResp.remove(0,9);
  tempResp = loRaRadio.base16decode(tempResp);
  if (tempResp == "" || tempResp.indexOf("CONNECT") >= 0)
  {
    connected = false;
  }
  return tempResp;
}

void change_frequency(String frequency){
  Serial.println("Changing frequency: " + frequency  + loRaRadio.sendRawCommand("radio set freq " + frequency));
}

void initialize_radio()
{
  //reset rn2483
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
  delay(500);
  digitalWrite(12, HIGH);

  delay(100); //wait for the RN2xx3's startup message
  loRaserial.flush();

  //Autobaud the rn2483 module to 9600. The default would otherwise be 57600.
  loRaRadio.autobaud();

  //check communication with radio
  String hweui = loRaRadio.hweui();
  while(hweui.length() != 16)
  {
    delay(500);
    Serial.println("Communication with RN2xx3 unsuccessful. Power cycle the board.");
    Serial.println(hweui);
    delay(10000);
    hweui = loRaRadio.hweui();
  }
  Serial.println(loRaRadio.sysver());

  RN2483_init();

}

bool connection_request(){
  change_frequency(RXfrequency);
  bool try_connecting = true;
  int connection_tries = 1;
  
  while(try_connecting && connection_tries<6) {
    Serial.println("Trying to connect " + String(connection_tries) + " times.");
    send_msg("CONNECT");

    resp = receive_message();

    if (resp.indexOf("CONNECTED") >=0 ){
      Serial.println("Connection created! Attempts: "+String(triedConnIndex));
      try_connecting = false;
      return true;
    }
    ++ connection_tries;
  }
  try_connecting = false;

  return false;
}

bool connection_protocol(){
  
  Serial.println("Waiting for connection to be established");
  change_frequency(TXfrequency);
  resp = receive_message();

  
  if(resp.indexOf("CONNECT")>=0){
    Serial.println("Connection requested. Trying to connect receiver");

    if(connection_request()){

      Serial.println("Connection confirmation received. Informing sender.");
      change_frequency(TXfrequency);
      send_msg(resp);
      connected = true;
      return true;
    }
    else{
      Serial.println("Connection failed. Informing sender.");
      send_msg("Couldn't connect receiver");

      return false;
    }
  }
  return false;
}

bool receiving_packets(){
   String tempS = "";
  if (!receive_packets){
      Serial.println("Can't receive packets");
      return false;
    }
  
  String snr = "";
  while(receive_packets){
      
    tempS = receive_message();
    snr = String(loRaRadio.getSNR());
    if (tempS.indexOf("P") >= 0)
    {
      packet = tempS;
      packet = tempS + ",BS:" + snr;
      Serial.println("BS Saved: " + packet);
    }

    if (tempS.indexOf("END") >= 0)
    { 
      Serial.println(tempS);
      Serial.println("Packets received!");
      receive_packets = false;
    }
    if (tempS.indexOf("CONNECT") >= 0 || tempS == "")
    {
      Serial.println("Failed receiving packages. Trying to reconnect TX");
      connected = false;
      return false;
    }  
  }
  return true;
}

// the setup routine runs once when you press reset:
void setup()
{
  //output LED pin
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600); //serial port to computer
  loRaserial.begin(57600); //serial port to radio
  Serial.println("Startup");

  initialize_radio();
  delay(100);
}


// the loop routine runs over and over again forever:
void loop() {
  while (!connected){
    connected = connection_protocol();
    status_led_connected(connected);
    if (connected){
      delay(500);
    }
  }
  while(connected){
    receive_packets = true;
    forward_packets = false;
    resp = "";
    resp = receive_message();
    Serial.println(resp);

    if (resp.indexOf("CONNECT") >= 0 ){
      connected = false;
      status_led_connected(connected);
    }
  
    if (resp.indexOf("START") >= 0 && receive_packets){
      status_led_receiving(receive_packets);
      forward_packets = receiving_packets();
      status_led_receiving(receive_packets);
      status_led_sending(forward_packets);
      
      if(forward_packets){
        change_frequency(RXfrequency);

        delay(1000);
        send_msg("START");

        delay(200);
        send_msg(packet);

        delay(200);
        send_msg("END");
        
        forward_packets = false;
        status_led_sending(forward_packets); 
      }

    if(!forward_packets && !receive_packets){
      resp = receive_message();
      if (resp.indexOf("P") >=0 ){
        change_frequency(TXfrequency);
        send_msg(resp);
        receive_packets = true;
        resp = "";
      }
    }  
    }
    else {
      change_frequency(TXfrequency);
      send_msg("Sending/receiving packets failed!");
      connected = false;
      status_led_connected(connected);
    }
  }  
}