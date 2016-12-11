//////////////////////////////////////////////
// KT0915 DSP radio
// Modified firmware for Digital Audio level control
// RSSI in FM mode is white color bar
//////////////////////////////////////////////
// Some Modifications by Adrian Wildangier
// Changes include: 
// - removed unused variables and renamed used variables for clarity & troubleshooting.
// - added a volume level for mono channels.  Including displaying which are mono modes.
// - added Serial() functions to debug code.  Use 9600 baud.
// - partially fixed bug with digital button reseting to default frequency, when tuning.  
// - modified SW modes to reflect Amateur Radio (ITU Region 2) frequencies.
// - bug added - need to hold digital button for one second, to properly reset frequency on mode change.
//////////////////////////////////////////////
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 9
Adafruit_SSD1306 display(OLED_RESET);
#define RADIO 0x35

bool digitalReadEvent, previousRotarySwitchState;
int readBits15_8, readBits7_0, savedBits;
int volumeBits = 0;
int displayCounter1, displayCounter2 = 0;
int terminal_1 = 2;
int terminal_2 = 4;
unsigned int modeSwitchEnabled;
volatile int rotarySwitchValue = 0;
volatile char rotaryDirectionState = 0;
volatile int modeSetting = 0; /// modeSetting = 0:AM, modeSetting = 1:FM

void writeI2C(int addressChip, int addressRegister, int writeBits15_8, int writeBits7_0)
{
  Wire.beginTransmission(addressChip);
  Wire.write(addressRegister);
    delay(5);
  Wire.write(writeBits15_8);
    delay(5);
  Wire.write(writeBits7_0);
    delay(5);
  Wire.endTransmission();

  Serial.print("- I2C Write;");
  Serial.print(" addrChip: 0x");
  Serial.print(addressChip, HEX);
  Serial.print(" addrReg: 0x");
  Serial.print(addressRegister, HEX);
  Serial.print(" Bits15_8: ");
  Serial.print(writeBits15_8, BIN);
  Serial.print(" Bits7_0: ");
  Serial.print(writeBits7_0, BIN);
  Serial.println();
}

void readI2C(int addressChip, int addressRegister)
{
  Wire.beginTransmission(addressChip);
  Wire.write(addressRegister);
  Wire.endTransmission(false);
  Wire.requestFrom(addressChip, 2);
  readBits15_8 = Wire.read();
  readBits7_0 = Wire.read();
  Wire.endTransmission(true);

  //Serial.print("- I2C Read;");
  //Serial.print(" addrChip: 0x");
  //Serial.print(addressChip, HEX);
  //Serial.print(" addrReg: 0x");
  //Serial.print(addressRegister, HEX);
  //Serial.print(" Bits15_8: ");
  //Serial.print(readBits15_8, BIN);
  //Serial.print(" Bits7_0: ");
  //Serial.print(readBits7_0, BIN);
  //Serial.println();
}

void setup()
{
  Wire.begin() ;
  attachInterrupt(0,rotaryDirection,CHANGE);
  attachInterrupt(1,rotaryPressed,CHANGE);
  delay(100) ;  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 
  pinMode(3, INPUT);
  pinMode(terminal_1, INPUT);
  pinMode(terminal_2, INPUT);

// Configure KA0915 Registers via I2C Start
// Set VOLUME (<15>FM Softmute/<14>AM Softmute/<5:4>10uF AC-Coupling Capacitor)
  writeI2C(RADIO,0x04,0b01100000,0b10110000);
// Set DSPCFGA (<5>Blend Enabled)
  writeI2C(RADIO,0x05,0b00010000,0b00100000); 
// Set RXCFG (<4:0>Volume Enabled)
  writeI2C(RADIO,0x0F,0b10001000,0b00010000);
// Set GPIOCFG
  writeI2C(RADIO,0x1D,0b00000000,0b00000100);
// Set AMDSP
  writeI2C(RADIO,0x22,0b10100010,0b11001100);
// Set SOFTMUTE
  writeI2C(RADIO,0x2E,0b00101000,0b10001100);
// Set AMCFG
  writeI2C(RADIO,0x33,0b11010100,0b00000001);
// Configure KA0915 Registers via I2C End  
 
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(32,32);
  display.print("DSP Radio");
  display.setCursor(32,40);
  display.print("v0.40");
  display.setCursor(32,48);
  display.print("Modified");
  display.setCursor(32,56);
  display.print("Firmware");
  display.display();
  
  delay(2000);
  display.setTextColor(BLACK);
  display.setCursor(32,32);
  display.print("DSP Radio");
  display.setCursor(32,40);
  display.print("v0.40");
  display.setCursor(32,48);
  display.print("Modified");
  display.setCursor(32,56);
  display.print("Firmware");
  display.display();
  display.setTextColor(WHITE);

  Serial.begin(9600);
  Serial.println("--- Start Serial Monitor Enabled");
  Serial.println();
}

void loop()
{
  // Display RSSI or Volume Update Routine per Second
  displayCounter1 = (millis() - 1000 * displayCounter2); 
  if(displayCounter1 > 500){
    displayCounter2++;
    displayFMRSSI();
    displayCounter1 = 0;
  }
  if(digitalReadEvent == HIGH) {
    if (modeSetting == 0) { modeFM(87.9);  } else 
    if (modeSetting == 1) { modeMW(540);   } else 
    if (modeSetting == 2) { modeSW(1600);  } else 
    if (modeSetting == 3) { modeSW(3500);  } else 
    if (modeSetting == 4) { modeSW(5350);  } else 
    if (modeSetting == 5) { modeSW(7000);  } else 
    if (modeSetting == 6) { modeSW(10100); } else 
    if (modeSetting == 7) { modeSW(14000); } else 
    if (modeSetting == 8) { modeSW(18060); } else 
    if (modeSetting == 9) { modeSW(21000); } else 
    if (modeSetting == 10){ modeSW(24890); } else 
    if (modeSetting == 11){ modeSW(28000); }
  }
}

void rotaryPressed()
{
  bool rotarySwitchState = 0; 
  //delay(100);
  rotarySwitchState = digitalRead(3);
   
  if (rotarySwitchState == LOW) {
    modeSetting = modeSetting + 1;
    previousRotarySwitchState = HIGH;
    
    //Serial.print("- RT Pressd;");
    //Serial.print(" butStateB: ");
    //Serial.print(rotarySwitchState);
    //Serial.print(" preStateB: ");
    //Serial.print(previousRotarySwitchState);
    //Serial.print(" switchVal: ");
    //Serial.print(rotarySwitchValue);
    //Serial.println();
  
  } else 
  if (rotarySwitchState == HIGH && previousRotarySwitchState == HIGH) {
    previousRotarySwitchState = LOW;
    rotarySwitchValue = 0; 
  
    //Serial.println(" switchVal: Reset");
  }
  if( modeSetting > 11){ modeSetting = 0;} 
  digitalReadEvent = HIGH;
  
  Serial.println("- RT Pressed");
}

void rotaryDirection(void)
{
  
  if(!digitalRead(terminal_1)){
    if(digitalRead(terminal_2)){
      rotaryDirectionState = 'R';
    } else {
      rotaryDirectionState = 'L';
    }
  } else {
    if(digitalRead(terminal_2)){
      if(rotaryDirectionState == 'L'){ 
        rotarySwitchValue++;
      }
    } else {
      if(rotaryDirectionState == 'R'){
        rotarySwitchValue--;
      }
    }

    digitalReadEvent = HIGH;
    
    //Serial.print("- RT Direct;");
    //Serial.print(" rotDirState: ");
    //Serial.println(char(rotaryDirectionState));
    
    rotaryDirectionState = 0;
  }
}

void displayFMRSSI()
{
  int maskStereo, maskPLL, previousBits;
  
  if(modeSetting == 0){
      readI2C(RADIO,0x12);
      previousBits = savedBits;
      
      savedBits = (-100 + (readBits7_0>>3) * 3);
      maskPLL = (readBits15_8 & 0b00001100);
      maskStereo = (readBits15_8 & 0b00000011);
      
      if(maskStereo == 0b11){
        display.setCursor(32,48);
        display.print("Stereo");
        display.display();
      } else {
        display.setCursor(32,48);
        display.print("      ");
        display.display();
      }
      if(maskPLL != 12){
        display.setCursor(32,48);
        display.print("PLL UNLOCK"); 
        display.display(); 
      }
      //if(previousBits != savedBits){
      //  display.setCursor(32,56);
      //  display.print("RSSI:");
      //  display.setCursor(70,56);
      //  display.setTextColor(BLACK);
      //  display.print(previousBits);
      //  display.display();
      //  display.setTextColor(WHITE); 
      //  display.setCursor(70,56);
      //  display.print(savedBits);
      //  display.display();
      //}
      if(previousBits != savedBits){
        display.fillRect(32,56,100 + previousBits,8,BLACK);
        display.display();
        display.setTextColor(WHITE); 
        display.fillRect(32,56,100 + savedBits,8,WHITE);
        display.display();
      }
  } else {
    previousBits = savedBits;
    
    readI2C(RADIO,0x0F);
    savedBits = (readBits7_0);
    
    display.setCursor(32,48);
    display.print("Mono");
    display.setCursor(32,56);
    display.print("Volume:");
    display.setTextColor(BLACK);
    display.setCursor(78,56);
    display.print(previousBits);
    display.setTextColor(WHITE);
    display.setCursor(78,56);
    display.print(savedBits);
    display.display();
  }
}

void modeSW(float startFreq)
{
  int radioFreq;
  unsigned int initialFreq, finalFreq, regBits15_8, regBits7_0;

  // Configure KA0915 Registers via I2C Start
  // Set AMSYSCFG (AM Mode/AFC Disabled)
  writeI2C(RADIO,0x16,0b10000000,0b00000011);
  // Set AMDSP (4KHz/Inverse the Left Audio)
  writeI2C(RADIO,0x22,0b10100010,0b10001100);  
  // writeI2C(RADIO,0x22,0b01010100,0b00000000);
  // Configure KA0915 Registers via I2C End
  initialFreq = startFreq;
  finalFreq = initialFreq + rotarySwitchValue * 5;
  regBits15_8 = (finalFreq>>8 | 0b10000000);
  regBits7_0 = (finalFreq & 0b11111111);
  writeI2C(RADIO,0x02,0b00000000,0b00000111);
  writeI2C(RADIO,0x17,regBits15_8,regBits7_0);
  
  radioFreq = finalFreq; 
  display.clearDisplay();  
  display.setCursor(32,32);
  display.print("SW (AM)");
  display.setCursor(32,40);
  display.print(radioFreq);
  display.setCursor(78,40);
  display.print("KHz ");
  display.display();
  digitalReadEvent = LOW; 
    
  Serial.print("- SW Values;");
  Serial.print(" initFreq: ");
  Serial.print(initialFreq);
  Serial.print(" finFreq: ");
  Serial.print(finalFreq);
  Serial.print(" rotSwiVal: ");
  Serial.print(rotarySwitchValue);
  Serial.print(" disFreq: - ");
  Serial.println(radioFreq);
}

void modeMW(float startFreq)
{
  float radioFreq;
  unsigned int initialFreq, finalFreq, regBits15_8, regBits7_0;
  
  // Configure KA0915 Registers via I2C Start
  // Set AMSYSCFG (AM Mode/AFC Disabled)
  writeI2C(RADIO,0x16,0b10000000,0b00000011);
  // writeI2C(RADIO,0x16,0b10000000,0b11000011);
    // Set AMDSP (4KHz/Inverse the Left Audio)
  writeI2C(RADIO,0x22,0b10100010,0b10001100);
  // writeI2C(RADIO,0x22,0b01010100,0b00000000);
  // Configure KA0915 Registers via I2C End
  initialFreq = startFreq;
  finalFreq = initialFreq + rotarySwitchValue * 10;
  regBits15_8 = (finalFreq>>8 | 0b10000000);
  regBits7_0 = (finalFreq & 0b11111111);
  writeI2C(RADIO,0x02,0b00000000,0b00000111);
  writeI2C(RADIO,0x17,regBits15_8,regBits7_0);
  
  radioFreq=finalFreq;
  display.clearDisplay();
  display.setCursor(32,32);
  display.print("MW (AM)");
  display.setCursor(32,40);
  display.print(radioFreq);
  display.setCursor(78,40);
  display.print("KHz ");
  display.display();
  digitalReadEvent = LOW; 

  Serial.print("- MW Values;");
  Serial.print(" initFreq: ");
  Serial.print(initialFreq);
  Serial.print(" finFreq: ");
  Serial.print(finalFreq);
  Serial.print(" rotSwiVal: ");
  Serial.print(rotarySwitchValue);
  Serial.print(" disFreq: - ");
  Serial.println(radioFreq);
}

void modeFM(float startFreq)
{
  float radioFreq;
  unsigned int initialFreq, finalFreq, regBits15_8, regBits7_0;
  
  // Configure KA0915 Registers via I2C Start
  // Set AMSYSCFG (FM Mode/-58dB Vol)
  writeI2C(RADIO,0x16,0b00000000,0b00000010);
  // Set GPIOCFG (Key Controlled Volume inc/dec)
  writeI2C(RADIO,0x1D,0b00000000,0b00000100);   
  // Configure KA0915 Registers via I2C End
  initialFreq = startFreq * 20.0;
  finalFreq = initialFreq + rotarySwitchValue * 2;
  regBits15_8 = (finalFreq>>8 | 0b10000000);
  regBits7_0 = (finalFreq & 0b11111111);
  writeI2C(RADIO,0x02,0b00000000,0b00000111);
  writeI2C(RADIO,0x03,regBits15_8,regBits7_0);
  
  radioFreq=finalFreq/20.0;
  display.clearDisplay();
  display.setCursor(32,32);
  display.print("FM (VHF)");
  display.setCursor(32,40);
  display.print(radioFreq);
  display.setCursor(70,40);
  display.print("MHz");
  display.display();
  digitalReadEvent = LOW;  
  
  Serial.print("- FM Values;");
  Serial.print(" initFreq: ");
  Serial.print(initialFreq);
  Serial.print(" finFreq: ");
  Serial.print(finalFreq);
  Serial.print(" rotSwiVal: ");
  Serial.print(rotarySwitchValue);
  Serial.print(" disFreq: - ");
  Serial.println(radioFreq);
}



