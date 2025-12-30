///////////////////////////////////////////////////////
//
// DCC packet analyze: Ruud Boer, October 2015
// DCC packet capture: Robin McKay, March 2014
// -------------------------------------------------------
// Version 2.0: Jürgen Winkler, March 2016
// - improve accessory decoder packet handling
// - improve detection of repeated = same packets
// - corrected display of on/off for basic accessory decoder packets
// - add hexdecimal output format for packets
// - packet output format may be selected using keyboard commands (bin,hex,both,none)
// - display of idle packet may be toggled using keyboard commands
// - add '?' help command displaying a list of available keyboard commands
// -------------------------------------------------------
//
// The DCC signal is detected on Arduino digital pin 2 - DCC signál se detekuje na pinu 2
//
// Set the Serial Monitor Baud Rate to 38400 !! Nastav rychlost Serial Monitoru na 38 400 baud!!
//
// Keyboard commands that can be sent via Serial Monitor:
// Klávesové příkazy, které mohou být zadány přes Serial Monitor:
//
// a = show accessory packets toggle - zobrazení paketů příslušenství (zapne/vypne)
// l = show locomotive packets toggle - zobrazení paketů lokomotivy (zapne/vypne)
// i = idle packet display on / off toggle - zobrazení idle paketů (zapne/vypne)
// h = hexadecimal output of packet data on / off toggle - zobrazení paketů hexadecimálně (zapne/vypne)
// b = binary output of packet data on / off toggle - zobrazení paketů binárně (zapne/vypne)
//
////////////////////////////////////////////////////////

#define TIMER_PRESCALER 64
#define DccBitTimerCount (F_CPU * 80L / TIMER_PRESCALER / 1000000L)
// 16000000 * 80 / 64 / 1000000 = 20; 20 x 4usecs = 80us

boolean packetEnd;
boolean preambleFound;

const byte bitBufSize = 50; // number of slots for bits
volatile byte bitBuffer[bitBufSize];
volatile byte bitBuffHead = 1;
volatile byte bitBuffTail = 0;

byte pktByteCount=0;
byte packetBytesCount;
byte preambleOneCount;
byte dccPacket[6]; // buffer to hold a packet
byte instrByte1;
byte decoderType; //0=Loc, 1=Acc
byte bufferCounter=0;
byte isDifferentPacket=0;
byte showLoc=1;
byte showAcc=1;

// --------------------added V2.0: begin ----------------
byte showIdle=0;
byte showBin=0;
byte showHex=1;
// --------------------added V2.0: end ----------------

unsigned int decoderAddress;

//========================

void getPacket() {
  preambleFound = false;
  packetEnd = false;
  packetBytesCount = 0;
  preambleOneCount = 0;
  while (! packetEnd) {
    if (preambleFound) getNextByte();
    else checkForPreamble();
  }
}

//========================

void checkForPreamble() {
   byte nextBit = getBit();
   if (nextBit == 1) preambleOneCount++;
   if (preambleOneCount < 10 && nextBit == 0) preambleOneCount = 0;
   if (preambleOneCount >= 10 && nextBit == 0) preambleFound = true;
}

//========================

void getNextByte() {
  byte newByte = 0;
  for (byte n = 0; n < 8; n++) newByte = (newByte << 1) + getBit();
  packetBytesCount ++;
  dccPacket[packetBytesCount] = newByte;
  dccPacket[0] = packetBytesCount;
  if (getBit() == 1) packetEnd = true;
}

//========================

byte getBit() {
  // gets the next bit from the bitBuffer
  // if the buffer is empty it will wait indefinitely for bits to arrive
  byte nbs = bitBuffHead;
  while (nbs == bitBuffHead) byte nbs = nextBitSlot(bitBuffTail); //Buffer empty
  bitBuffTail = nbs;
  return (bitBuffer[bitBuffTail]);
}

//========================

void beginBitDetection() {
  TCCR0A &= B11111100;
  attachInterrupt(0, startTimer, RISING);
}

//========================

void startTimer() {
  OCR0B = TCNT0 + DccBitTimerCount;
  TIMSK0 |= B00000100;
  TIFR0  |= B00000100;
}

//========================

ISR(TIMER0_COMPB_vect) {
  byte bitFound = ! ((PIND & B00000100) >> 2);
  TIMSK0 &= B11111011;
  byte nbs = nextBitSlot(bitBuffHead);
  if (nbs == bitBuffTail) return;
  else {
    bitBuffHead = nbs;
    bitBuffer[bitBuffHead] = bitFound;
  }
}

//========================

byte nextBitSlot(byte slot) {
  slot ++;
  if (slot >= bitBufSize) slot = 0;
  return(slot);
}

//========================

void printPacket() {
  // changed V2.0: show binary output is optional now
  if (showBin || showHex) {
    Serial.print(" Data");
  }
  if (showBin) {
    for (byte n=1; n<pktByteCount; n++) {
      Serial.print(" ");
      Serial.print(dccPacket[n],BIN);
    }
  }

  // --------------------added V2.0: begin ----------------
  // optionally show hexadecimal output now
  if (showHex) {
    Serial.print(" Hex");
    for (byte n=1; n<pktByteCount; n++) {
      Serial.print(" ");
      Serial.print(dccPacket[n],HEX);
    }
  }
  // --------------------added V2.0: end ----------------

  Serial.println();
}

//========================

void setup() {
  Serial.begin(115200); // 38400 when on DCC, 9600 when testing on 123Circuits !!!!!!!!!!!!!!!!!!!!!!!
  Serial.println(F("---"));
  Serial.println(F("DCC Packet Analyze V2.0 started"));
  Serial.println(F("---"));
  Serial.println(F("press ? for help"));      // added V2.0
  beginBitDetection(); //Uncomment this line when on DCC !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW); //on board LED off
}

// added V2.0: start
void displayCV(byte mode, short cvAddress, byte cvValue) {
  switch(mode) {
    case 1: Serial.print("Verify "); break;
    case 2: Serial.print("Bit "); break;
    case 3: Serial.print("Write "); break;
    case 10: Serial.print("Bit write "); break;
    case 11: Serial.print("Bit verify "); break;
    default: Serial.print("reserved "); break;
  }
  Serial.print("Addr ");
  Serial.print(cvAddress);
  if (mode==2 || mode==10 || mode==11) {  // bit opertion
    Serial.print(" Bit ");
    Serial.print(dccPacket[pktByteCount-1]&B00000111);
    Serial.print("=");
    Serial.print((dccPacket[pktByteCount-1]&B00001000)>>3);
  } else {  // byte operation
    Serial.print(" Value ");
    Serial.print(cvValue);
  }
}
// added V2.0: end

//====================

void loop() {
  getPacket(); //Uncomment this line when on DCC !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  byte speed;
  byte checksum = 0;
  unsigned long packetHash = 0;      // added V2.0

/* Dummy packet for test purposes. Comment when on DCC !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Loc 1782 CV Write 3 128
dccPacket[0]=6;
dccPacket[1]=B11000111;
dccPacket[2]=B01101110;
dccPacket[3]=B11101100;
dccPacket[4]=B00000011;
dccPacket[5]=B10000000;
dccPacket[6]=B11111111;
*/
  checkUserInput();    // changed v2.0: check user input anyway, also on idle packet and with packet having checksum error

  pktByteCount = dccPacket[0];
  if (!pktByteCount) return; // No new packet available

  for (byte n = 1; n <= pktByteCount; n++) checksum ^= dccPacket[n];
  //checksum=0; //Comment this line when on DCC !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (checksum) {
    Serial.print(millis());
    Serial.println(" Invalid checksum!");
    return; // Invalid Checksum
  }

  if (dccPacket[1]==B11111111) { //Idle packet
    if (showIdle) {
      Serial.print(millis());
      Serial.println(" Idle ");
    }
    return;
  }

  if (!bitRead(dccPacket[1],7)) { //bit7=0 -> Loc Decoder Short Address
    decoderAddress = dccPacket[1];
    instrByte1 = dccPacket[2];
    decoderType = 0;
  }
  else {
    if (bitRead(dccPacket[1],6)) { //bit7=1 AND bit6=1 -> Loc Decoder Long Address
      decoderAddress = 256 * (dccPacket[1] & B00111111) + dccPacket[2];
      instrByte1 = dccPacket[3];
      decoderType = 0;
    }
    else { //bit7=1 AND bit6=0 -> Accessory Decoder
      decoderAddress = dccPacket[1]&B00111111;
      instrByte1 = dccPacket[2];
      decoderType = 1;
    }
  }

  if (decoderType) { // Accessory Basic
    if (showAcc) {
      if (instrByte1&B10000000) { // Basic Accessory Decoder Packet Format
        decoderAddress = (((~instrByte1)&B01110000)<<2) + decoderAddress;
        byte port = (instrByte1&B00000110)>>1;
        Serial.print(millis());
        Serial.print(" Acc ");
        Serial.print((decoderAddress-1)*4 + port + 1);
        Serial.print(" ");
        Serial.print(decoderAddress);
        Serial.print(":");
        Serial.print(port);
        if (pktByteCount==3) {  // Basic Accessory Decoder Packet Format
          Serial.print(" output ");     // changes V2.0: bits 0 indicates which output of the pair is addresse
          Serial.print(bitRead(instrByte1,0));
          if (bitRead(instrByte1,3)) Serial.print(" On");  // changes V2.0 bits 3 is used to activate or deactivate the port
          else Serial.print(" Off");
        }
        // --------------------added V2.0: begin ----------------
        // handle decoder CV access
        else if (pktByteCount==5 && (dccPacket[2]&B10001100)==B00001100) {  // Accessory decoder Configuration variable Instruction - backward compatibility
          Serial.print(" CV ");
          int cvAddress = 1 + dccPacket[3] + (dccPacket[2]&B00000011)*256;
          byte cvValue = dccPacket[4];
          displayCV(3, cvAddress, cvValue);
        }
        else if (pktByteCount==6 && (dccPacket[3]&B11110000)==B11100000) {  // Accessory decoder Configuration variable Instruction
          Serial.print(" CV ");
          byte cvMode = (dccPacket[3]&B00001100)>>2;
          int cvAddress = 1 + dccPacket[4] + (dccPacket[3]&B00000011)*256;
          byte cvValue = dccPacket[5];
          displayCV(cvMode, cvAddress, cvValue);
        }
        else {
          Serial.print(" unknown ");
        }
        // --------------------added V2.0: end ----------------
      }
      else { // Accessory Extended NMRA spec is not clear about address and instruction format !!!
        Serial.print("Acc Ext ");
        decoderAddress = (decoderAddress<<5) + ((instrByte1&B01110000)>>2) + ((instrByte1&B00000110)>>1);
        Serial.print(decoderAddress);
        Serial.print(" Asp ");
        Serial.print(dccPacket[3],BIN);
      }
      printPacket();
    }
  }
  else { // Loc / Multi Function Decoder
    if (showLoc) {
      Serial.print(millis());
      Serial.print(" Loc ");
      Serial.print(decoderAddress);
      byte instructionType = instrByte1>>5;
      switch (instructionType) {

        case 0:
          Serial.print(" Control ");
        break;

        case 1: // Advanced Operations
          if (instrByte1==B00111111) { //128 speed steps
            if (bitRead(dccPacket[pktByteCount-1],7)) Serial.print(" Forw128 ");
            else Serial.print(" Rev128 ");
            byte speed = dccPacket[pktByteCount-1]&B01111111;
            if (!speed) Serial.print(" Stop ");
            else if (speed==1) Serial.print(" E-stop ");
            else Serial.print(speed-1);
          }
          else if (instrByte1==B00111110) { //Speed Restriction
          if (bitRead(dccPacket[pktByteCount-1],7)) Serial.print(" On ");
            else Serial.print(" Off ");
            Serial.print(dccPacket[pktByteCount-1])&B01111111;
          }
        break;

        case 2: // Reverse speed step
          speed = ((instrByte1&B00001111)<<1) - 3 + bitRead(instrByte1,4);
          if (speed==253 || speed==254) Serial.print(" Stop ");
          else if (speed==255 || speed==0) Serial.print(" E-Stop ");
          else {
            Serial.print(" Rev ");
            Serial.print(speed);
          }
        break;

        case 3: // Forward speed step
          speed = ((instrByte1&B00001111)<<1) - 3 + bitRead(instrByte1,4);
          if (speed==253 || speed==254) Serial.print(" Stop ");
          else if (speed==255 || speed==0) Serial.print(" E-Stop ");
          else {
            Serial.print(" Forw ");
            Serial.print(speed);
          }
        break;

        case 4: // Loc Function L-4-3-2-1
          Serial.print(" L F4-F1 ");
          Serial.print(instrByte1&B00011111,BIN);
        break;

        case 5: // Loc Function 8-7-6-5
          if (bitRead(instrByte1,4)) {
            Serial.print(" F8-F5 ");
            Serial.print(instrByte1&B00001111,BIN);
          }
          else { // Loc Function 12-11-10-9
            Serial.print(" F12-F9 ");
            Serial.print(instrByte1&B00001111,BIN);
          }
        break;

        case 6: // Future Expansions
          switch (instrByte1&B00011111) {
            case 0: // Binary State Control Instruction long form
              Serial.print(" BinStateLong ");
              Serial.print(256 * dccPacket[pktByteCount-1] + (dccPacket[pktByteCount-2]&B01111111));
              if bitRead(dccPacket[pktByteCount-2],7) Serial.print(" On ");
              else Serial.print(" Off ");
            break;
            case B00011101: // Binary State Control
              Serial.print(" BinStateShort ");
              Serial.print(dccPacket[pktByteCount-1]&B01111111);
              if bitRead(dccPacket[pktByteCount-1],7) Serial.print(" On ");
              else Serial.print(" Off ");
            break;
            case B00011110: // F13-F20 Function Control
              Serial.print(" F20-F13 ");
              Serial.print(dccPacket[pktByteCount-1],BIN);
            break;
            case B00011111: // F21-F28 Function Control
              Serial.print(" F28-F21 ");
              Serial.print(dccPacket[pktByteCount-1],BIN);
            break;
          }
        break;

        case 7:
          Serial.print(" CV ");
          if (instrByte1&B00010000) { // CV Short Form
            byte cvType=instrByte1&B00001111;
            switch (cvType) {
              case B00000010:
                Serial.print("23 ");
                Serial.print(dccPacket[pktByteCount-1]);
              break;
              case B00000011:
                Serial.print("24 ");
                Serial.print(dccPacket[pktByteCount-1]);
              break;
              case B00001001:
                Serial.print("Decoder Lock ");
                Serial.print(dccPacket[pktByteCount-1]);
              break;
            }
          }
          else { // CV Long Form
            int cvAddress = 256 * (instrByte1&B00000011) + dccPacket[pktByteCount-2] + 1;
            byte cvMode = (instrByte1&B00001100)>>2;
            byte cvData = dccPacket[pktByteCount-1];
            switch (cvMode) {
              case 1: // Verify Byte
              case 3: // Write Byte
                displayCV(cvMode, cvAddress, cvData);
              break;
              case 2: // Bit Write
                if (dccPacket[pktByteCount-2]&B00010000) displayCV(11, cvAddress, cvData); // bit verify
                else displayCV(11, cvAddress, cvData); // bit write
              break;
            }
          }
        break;
      }
      printPacket();
    }
  }
}

// --------------------changed V2.0: begin ----------------
// refactor code - extrace own method for handling user input
// add handling of 'i', 'h', 'b' and '?' keys
// turn off interrupts while keyboard handling, sometimes serial.print last too long and the app crashes
// use F() macro to save memory in data memeory -> strings are now in program memory

void checkUserInput() {
  if (Serial.available()) {
    noInterrupts();            // changed V2: turn off interrupts, otherwise if serial.print last too long the app crashes
    Serial.println(" ");
    switch (Serial.read()) {
      case 97:
        if (showAcc) showAcc=0; else showAcc=1;
        Serial.print(F("show acc packets = "));
        Serial.println(showAcc);
      break;
      case 108:
        if (showLoc) showLoc=0; else showLoc=1;
        Serial.print(F("show loc packets = "));
        Serial.println(showLoc);
      break;
      case 105:
        if (showIdle) showIdle=0; else showIdle=1;
        Serial.print(F("show idle packets = "));
        Serial.println(showIdle);
      break;
      case 104:
        if (showHex) showHex=0; else showHex=1;
        Serial.print(F("show packet data in hexadezimal = "));
        Serial.println(showHex);
      break;
      case 98:
        if (showBin) showBin=0; else showBin=1;
        Serial.print(F("show packet data in binary = "));
        Serial.println(showBin);
      break;
      case 63: // ? show help
        Serial.println(F("Available keyboard commands:"));
        Serial.println(F("  1 = 1s refresh time"));
        Serial.println(F("  2 = 2s"));
        Serial.println(F("  3 = 4s (default)"));
        Serial.println(F("  4 = 8s"));
        Serial.println(F("  5 = 16s"));
        Serial.println();
        Serial.println(F("  6 = 4 DCC packet buffer size"));
        Serial.println(F("  7 = 8"));
        Serial.println(F("  8 = 16"));
        Serial.println(F("  9 = 32 (default)"));
        Serial.println(F("  0 = 64"));
        Serial.println();
        Serial.println(F("  a = accessory packets display on / off toggle"));
        Serial.println(F("  l = locomotive packets display on / off toggle"));
        Serial.println(F("  i = idle packet display on / off toggle"));
        Serial.println();
        Serial.println(F("  h = hexadecimal output of packet data on / off toggle"));
        Serial.println(F("  b = binary output of packet data on / off toggle"));
      }
    Serial.println(" ");
    interrupts();       // changed V2: turn interupts on again
  }
}
// --------------------changed V2.0: end ----------------

//=====================

