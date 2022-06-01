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
#include <time.h>
#define START_MESSAGE "START"
#define END_MESSAGE   "END"

int sndMsgIndex = 0;
int triedConnIndex = 1;
int succesfull_transmissions = 0;
int tried_transmissions = 0;

unsigned long full_time = 0;
unsigned long send_time = 0;

bool connected = false;

String resp;
String conf;

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
    "radio set wdt 12000",      //Watch-dog timeout time in ms
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
    Serial.println("Connected");
  }
  else {
    digitalWrite(4, LOW);
    Serial.println("Not connected");
  }
}

void status_led_sending(boolean ledStatus)
{
  if (ledStatus){
    digitalWrite(3, HIGH);
    Serial.println("Sending");
  }
  else {
    digitalWrite(3, LOW);
    Serial.println("Not sending");
  }
}

void status_led_receiving(boolean ledStatus)
{
  if (ledStatus){
    digitalWrite(2, HIGH);
    Serial.println("Receiving");
  }
  else {
    digitalWrite(2, LOW);
    Serial.println("Not receiving");
  }
}


void RN2483_init(){
  Serial.println("Initializing LoRa module");
  int index = 0;
  while (index<=15){
    Serial.println("Commad: " + initCommands[index] +": " + loRaRadio.sendRawCommand(initCommands[index]));
    ++ index;
  }
  
}

void send_msg(String data){
  Serial.println("Sending message: " + data);
  data = loRaRadio.base16encode(data);
  Serial.println(loRaRadio.sendRawCommand("radio tx " + data));
  Serial.println(loRaserial.readStringUntil('\n'));
  //delay(200);
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


// the setup routine runs once when you press reset:
void setup()
{
  //output LED pin
  //output LED pin
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  

  // Open serial communications and wait for port to open:
  Serial.begin(9600); //serial port to computer
  loRaserial.begin(57600); //serial port to radio
  Serial.println("Startup");

  initialize_radio();
  delay(2000);
}

bool connection_request(){
  bool try_connecting = true;
  int connection_tries = 1;
  
  while(connection_tries < 6 && try_connecting) {
    Serial.println("Trying to connect " + String(connection_tries) + " times.");
    send_msg("CONNECT");

    Serial.println("radio rx 0: " + loRaRadio.sendRawCommand("radio rx 0"));
    
    while(loRaserial.available()== 0){ 
    }
    resp = loRaserial.readStringUntil('\n');

    resp.remove(0,9);
    resp = loRaRadio.base16decode(resp);
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

bool send_packets(){
  delay(500);
  ++ tried_transmissions;
  String packet = "";

  delay(200);   
  packet = START_MESSAGE;
  send_msg(packet);

  delay(200);    
  packet = "P/" + String(tried_transmissions);
  send_msg(packet); 
  
  delay(200);   
  packet = END_MESSAGE;
  send_msg(packet);

  return true;
}

String receive_message(){
  Serial.println("radio rx 0: " + loRaRadio.sendRawCommand("radio rx 0"));
  while(loRaserial.available() == 0){
    }
  String tempResp = loRaserial.readStringUntil('\n');
  tempResp.remove(0,9);
  return loRaRadio.base16decode(tempResp);
}

void loop() {
  if(full_time == 0){
    full_time = millis();
  }
  
  while (!connected)
  { status_led_connected(connected);
    Serial.println("Establishing connection " +String(triedConnIndex) + " times");
    connected = connection_request();
    
    ++triedConnIndex;
  }
  // Start sending packets
  if(connected){
    status_led_connected(connected);
    resp = "";
    conf = "";
    delay(500);
    if(send_packets()){
      
      Serial.println("Waiting for received confirmation");
      delay(1000);
      conf = receive_message();
      Serial.println("conf: " + conf);

      if (conf.indexOf("P") >=0 ){
        ++ succesfull_transmissions;
        Serial.println(conf);
        Serial.println("Succesfully send. Succesfull transmissions: "+ String(succesfull_transmissions) + ", Tried transmissions: "  + String(tried_transmissions));

      }
      else{
        
        Serial.println("Failed to get confirmation. Tried transmissions: " + String(tried_transmissions));
        connected = false;
        status_led_connected(connected);
      }
    }
  }  
}
