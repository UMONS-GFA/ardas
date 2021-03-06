/*** ##### WARNING : to be used with Ardas_shield 01_2016 ##### ***/
/***
    Copyright (C) 2013-2016 UMONS

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>

***/



#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#define PULSE_WIDTH_USEC 5
#define READ_COUNTER_REGISTER_FREQ 2 // CHECK : should be 1 if SQW freq is 1 Hz
#define CLOCK_FREQ 4096
#define VERSION "Version ArDAS 1.0.1 [UMONS-GFA - 2016]"
#define EOL '\r'
#define ADDR_STATION 0
#define ADDR_NETID 2
#define ADDR_SAMPLING_RATE 3
#define ADDR_NB_INST 5
#define ADDR_I1 7
#define ADDR_I2 9
#define ADDR_I3 11
#define ADDR_I4 13
#define ADDR_RASPARDAS_MODE 15

#define PEER_DOWNLOAD_MODE 16
#define QUIET_MODE 17

const uint32_t PROGMEM crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

const uint16_t COUNTS_BETWEEN_READINGS = CLOCK_FREQ / READ_COUNTER_REGISTER_FREQ;
const uint8_t NUMBER_OF_COUNTERS = 2; // 32 bits counters
const uint8_t NUMBER_OF_CHANNELS_PER_COUNTER = 2; // 1 : 32 bits channels - 2: 16 bits channels
const uint8_t NUMBER_OF_CHANNELS = NUMBER_OF_COUNTERS * NUMBER_OF_CHANNELS_PER_COUNTER;
const uint8_t BYTES_PER_CHANNEL = 4 / NUMBER_OF_CHANNELS_PER_COUNTER;

boolean DEBUG = false; // true;
boolean quiet = true; //
uint8_t freq = CLOCK_FREQ;

RTC_DS1307 RTC;

// Pins
/* RTC */
// Don't forget to connect I2C pins A4 and A5 (5 and 6 of RTC)
// rtc_pulse on pin 2 for interrupt 0
/* SN74LV8054 */
const uint8_t rclk_pin = 14;       // set selected byte to output
/* 74HC165N */
const uint8_t pl_pin = 8;         // latch pin
const uint8_t cp_pin = 15;         // clock for synchronous communication
const uint8_t q7_pin = 7;         // serial output
const uint8_t ce_pin = 9;         // activation cp clock
/* 74HC595N */
const uint8_t stcp_pin = 6;       // pin connected to ST_CP of 74HC595
const uint8_t shcp_pin = 3;       // pin connected to SH_CP of 74HC595
const uint8_t ds_pin = 5;         // pin connected to DS of 74HC595


//others
volatile uint16_t pulse_counter = 0;
volatile boolean read_counter_register = false;
uint32_t channel[NUMBER_OF_CHANNELS];
uint32_t c1=0;
uint32_t counter_overflow;
uint32_t previous_count[NUMBER_OF_CHANNELS];
uint32_t current_count[NUMBER_OF_CHANNELS];
uint8_t b[NUMBER_OF_COUNTERS]; // incoming bytes
uint8_t cn = 0;
uint16_t n = 0;
boolean start_flag = true;
boolean peer_download = false;
boolean raspardas_mode = false;
uint8_t nfe = 0;
uint16_t sampling_rate = 2; // integration time in seconds
Ds1307SqwPinMode modes[] = {SquareWave1HZ, SquareWave4kHz, SquareWave8kHz, SquareWave32kHz};
Ds1307SqwPinMode RTC_freq;

// µDAS interface
uint8_t netid, nb_inst;
uint16_t station;
uint16_t inst[NUMBER_OF_CHANNELS];
uint8_t echo = 0;
boolean download_flag = false;
uint8_t record[33];
uint8_t crc[4];
uint32_t dc[NUMBER_OF_CHANNELS];
uint32_t fc[NUMBER_OF_CHANNELS];


String s;

void rtc_interrupt()
{
    //noInterrupts(); // TODO : check if needed
    // after each integration period
    if (pulse_counter % COUNTS_BETWEEN_READINGS == 0){
        digitalWrite(rclk_pin, HIGH);
        digitalWrite(rclk_pin, LOW);

        read_counter_register = true;

    }
    pulse_counter ++;
    //interrupts(); // TODO : check if needed
}

//void(* reset) (void) = 0;//declare reset function at address 0
/* alternative possibly better one using the watchdog...
check ram usage...
*/
void software_reset()
{
    wdt_enable(WDTO_15MS);
    while(1)
    {
    }
}

uint32_t crc_update(uint32_t crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
}

uint32_t crc_string(char *msg, size_t arraySize)
{
  uint32_t crc = ~0L;
  for (int i=0; i < arraySize; i++) {
    crc = crc_update(crc, *msg++);
  }
  crc = ~crc;
  return crc;
}

void SerialPrintf(char *format,...)
{
    char buff[80];
    va_list args;
    va_start (args,format);
    vsnprintf(buff,sizeof(buff),format,args);
    va_end (args);
    buff[sizeof(buff)/sizeof(buff[0])-1]='\0';
    Serial.print(buff);
}

void read_shift_regs()
{
    uint32_t bitVal;  // check type

    digitalWrite(ce_pin, HIGH);
    digitalWrite(pl_pin, LOW);
    delayMicroseconds(PULSE_WIDTH_USEC);
    digitalWrite(pl_pin, HIGH);
    digitalWrite(ce_pin, LOW);
    for(int j = 0; j < NUMBER_OF_COUNTERS; j++){
        b[j] = 0;
        for(int i = 0; i < 8; i++)
        {
            bitVal = (uint32_t) digitalRead(q7_pin);
            b[j] |= (bitVal << (7 - i));
            digitalWrite(cp_pin, HIGH);
            delayMicroseconds(PULSE_WIDTH_USEC);
            digitalWrite(cp_pin, LOW);
        }
    }
}

void digitalSerialWrite(uint8_t Val)
{
    digitalWrite(stcp_pin, LOW);
    shiftOut(ds_pin, shcp_pin, MSBFIRST, Val);
    digitalWrite(stcp_pin, HIGH);
}

void read_bytes(uint8_t byte_number) {
    uint8_t val;

    switch(byte_number){
        case 0:
            val = 119;
            break;
        case 1:
            val = 187;
            break;
        case 2:
            val = 221;
            break;
        case 3:
            val = 238;
            break;
    }
    digitalSerialWrite(val);
    delayMicroseconds(PULSE_WIDTH_USEC);
    read_shift_regs();
    delayMicroseconds(PULSE_WIDTH_USEC);
}

// µDAS interface
void EEPROMWriteOnTwoBytes(uint8_t address, uint8_t value) {
    uint8_t two = (value & 0xFF);
    uint8_t one = ((value >> 8) & 0xFF);

    EEPROM.write(address, two);
    EEPROM.write(address +1, one);
}

uint16_t EEPROMReadTwoBytes (uint8_t address) {
    uint8_t two = EEPROM.read (address);
    uint8_t one = EEPROM.read (address + 1);

    return ((two << 0) & 0xFF) + ((one << 8) & 0xFFFF);
}

// Get SRAM usage
int freeRam() {
    extern int __heap_start, * __brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start:(int) __brkval);
}

void read_config_or_set_default() {
    station = EEPROMReadTwoBytes(ADDR_STATION);
    if (station == 0) {
        station = 1;
    }
    netid = EEPROM.read(ADDR_NETID);
    if (netid == 0) {
        netid = 255;
    }
    sampling_rate = EEPROMReadTwoBytes(ADDR_SAMPLING_RATE);
    if (sampling_rate == 0) {
        sampling_rate = 60;
    }
    nb_inst = EEPROM.read(ADDR_NB_INST);
    if ( nb_inst == 0) {
        nb_inst = NUMBER_OF_CHANNELS;
    }
}

void connect() {
    Serial.print(F("!HI "));
    SerialPrintf("%04d %03d %04d %1d %04d %04d %04d %04d 000000 133256 ", station, netid, sampling_rate, nb_inst, inst[0], inst[1], inst[2], inst[3]); // TODO implement nb_inst!=4 + memory usage
    Serial.print(F("\n\r"));
    echo = 2;

}

void help() {
    Serial.print(F("\n\rHELP COMMAND :\n\r\n\r"));
    Serial.print(F("#E0 : No Echo\n\r\n\r"));
    Serial.print(F("#E1 : Only Data\n\r\n\r"));
    Serial.print(F("#E2 : Data + Time\n\r\n\r"));
    Serial.print(F("#SD yyyy mm dd hh nn ss : Set Date + Time\n\r\n\r"));
    Serial.print(F("#SR iiii : Set Integration Period\n\r\n\r"));
    Serial.print(F("#SI nnn : Set uDAS Number\n\r\n\r"));
    Serial.print(F("#SS ssss : Set Station Number\n\r\n\r"));
    Serial.print(F("#RI : Read Info\n\r\n\r"));
    Serial.print(F("#RL : Read Last Data\n\r\n\r")); // TODO : implement this
    Serial.print(F("#RM : Read Memory Status\n\r\n\r")); // TODO : implement this
    Serial.print(F("#RV : Read version\n\r\n\r")); // NOTE : not present on uDAS v3.05
    Serial.print(F("#ZR ssss nnn iiii s 1111 2222 3333 4444 XX: Reconfig\n\r\n\r"));
    Serial.print(F("#XB : Full Download\n\r\n\r"));
    Serial.print(F("#XP yyyy mm dd hh nn ss yyyy mm dd hh nn ss : Partial Download\n\r\n\r")); // NOTE : implement this
    Serial.print(F("#XN : Next Download\n\r\n\r")); // NOTE : implement this
    Serial.print(F("#WB : Write line in workbook\n\r\n\r")); // NOTE : implement this
    Serial.print(F("#RW : Read workbook\n\r\n\r")); // NOTE : implement this
    Serial.print(F("#?? : This text\n\r\n\r")); // NOTE : implement this
    Serial.print(F("Enter your command : "));
}

void set_no_echo() {
    Serial.println(F("\n\r!E0 [Echo disabled]\n\r"));
    echo = 0;
}

void set_echo_data() {
    Serial.println(F("!E1\n\r"));
    echo = 1;
}

void set_echo_data_and_time() {
    Serial.println(F("!E2\n\r"));
    echo = 2;
}

void set_date_and_time(String s) {
    uint16_t yr;
    uint8_t mh, dy, hr, mn, sc;

    if (s.length() == 24) {
        yr = (uint16_t) s.substring(4, 8).toInt();
        mh = (uint8_t) s.substring(9, 11).toInt();
        dy = (uint8_t) s.substring(12, 14).toInt();
        hr = (uint8_t) s.substring(15, 17).toInt();
        mn = (uint8_t) s.substring(18, 20).toInt();
        sc = (uint8_t) s.substring(21, 23).toInt();
        RTC.adjust(DateTime(yr, mh, dy, hr, mn, sc));
        RTC.writeSqwPinMode(RTC_freq);
        Serial.print(F("!SD "));
        SerialPrintf("%04d %02d %02d %02d %02d %02d", yr, mh, dy, hr, mn, sc);
        Serial.print(F("\n\r"));
    }
    else {
        Serial.print(F("!SD value error\n\r"));
    }
}

void get_das_info() {
    Serial.print(F("!RI"));
    Serial.print(F(" Station:"));
    SerialPrintf("%04d", station);
    Serial.print(F(" DasNo:"));
    SerialPrintf("%03d", netid);
    Serial.print(F(" Integration:"));
    SerialPrintf("%04d", sampling_rate);
    for (int i=0;i<4;i++){
      Serial.print(F(" I"));
      Serial.print(i+1);
      Serial.print(F(":"));
      SerialPrintf("%04d",inst[i]);
    }
    Serial.print(F(" 367959 3997696 1442365200")); // TODO : manage memory
    Serial.print(F("\n\r"));
}

void get_raspardas_mode(){
    byte val = EEPROM.read(ADDR_RASPARDAS_MODE);
    if (val == 0xFF){
        raspardas_mode = true;
    }
    else {
        raspardas_mode = false;
    }
}

void get_version() {
    Serial.print(F("!RV "));
    Serial.print(VERSION);
    Serial.print(F("\n\r"));
}


void set_station_id(String s) {
    if (s.length() == 9) {
        station = (uint16_t) s.substring(4, 8).toInt();
        EEPROMWriteOnTwoBytes(ADDR_STATION, station);
        Serial.print(F("! SS "));
        SerialPrintf("%04d", station);
        Serial.print(F("\n\r"));
    }
    else {
        Serial.print(F("!SS value error\n\r"));
    }
}

void set_das_netid(String s) {
    if(s.length() == 8) {
        netid = (uint8_t) s.substring(4,7).toInt();
        EEPROM.write(ADDR_NETID, netid);
    }
    Serial.print(F("!SI "));
    SerialPrintf("%03d",netid);
    Serial.print(F("\n\r"));
}

void set_sampling_rate(String s) {
    if (s.length() == 9) {
        // TODO: check parameter type
        sampling_rate = (uint16_t) s.substring(4,8).toInt(); // TODO unsigned int
        EEPROMWriteOnTwoBytes(ADDR_SAMPLING_RATE, sampling_rate);
        start_flag = true;
    }
    Serial.print(F("!SR "));
    SerialPrintf("%04d", sampling_rate);
    Serial.print(F("\n\r"));
}

void set_raspardas_mode(boolean val){
    if (val) {
        EEPROM.write(ADDR_RASPARDAS_MODE, 0xFF);
    }
    else {
        EEPROM.write(ADDR_RASPARDAS_MODE, 0x00);
    }
    raspardas_mode = val;
}

void reconfig(String s){ // NOTE : hard coded for 4 instruments
    if (s.length() == 43) {
        station = (uint16_t) s.substring(4, 8).toInt();
        netid = (uint8_t) s.substring(9, 12).toInt();
        sampling_rate = (uint16_t) s.substring(13, 17).toInt();
        nb_inst = (uint8_t) s.substring(18, 19).toInt();
        inst[0] = (uint16_t) s.substring(20,24).toInt();
        inst[1] = (uint16_t) s.substring(25,29).toInt();
        inst[2] = (uint16_t) s.substring(30,34).toInt();
        inst[3] = (uint16_t) s.substring(35,39).toInt();

        if (s.substring(40, 42).toInt() == 31) {
          EEPROMWriteOnTwoBytes(ADDR_STATION, station);
          EEPROM.write(ADDR_NETID, netid);
          EEPROMWriteOnTwoBytes(ADDR_SAMPLING_RATE, sampling_rate);
          EEPROM.write(ADDR_NB_INST, nb_inst);
          EEPROMWriteOnTwoBytes(ADDR_I1, inst[0]);
          EEPROM.write(ADDR_I1, inst[0]);
          EEPROMWriteOnTwoBytes(ADDR_I2, inst[1]);
          EEPROM.write(ADDR_I2, inst[1]);
          EEPROMWriteOnTwoBytes(ADDR_I3, inst[2]);
          EEPROM.write(ADDR_I3, inst[2]);
          EEPROMWriteOnTwoBytes(ADDR_I4, inst[3]);
          EEPROM.write(ADDR_I4, inst[3]);
          delay(100);
        }
        Serial.print(F("!ZR "));
        SerialPrintf("%04d", station);
        Serial.print(F(" "));
        SerialPrintf("%03d", netid);
        Serial.print(F(" "));
        SerialPrintf("%04d", sampling_rate);
        for (int i=0;i<4;i++){
          Serial.print(F(" "));
          SerialPrintf("%04d",inst[i]);
        }
        Serial.print(F("\n\r"));
        start_flag = true;
    }
    else{
        Serial.print(F("!ZR value error\n\r"));
    }
}


/***
SETUP
***/

void setup()
{
    Serial.begin(57600);
    Serial.flush();
    s = "#ZR 0001 255 0001 4 0000 0000 0000 0000 31\r";
    reconfig(s);
    set_raspardas_mode(true);
}

/***
LOOP
***/

void loop(){
}
