#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <avr/pgmspace.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

const byte decA0=2, decA1=3, aOhm=A0, aAmp=A1, aVolt=A2;
const byte btnR=6, btnOK=7, btnL=9, btnB=10, sysBuz=8;
const byte pD0=0, pD1=1, pD2=5, pD3=4, pD4=11, pD5=12, pD6=13, pA0=A3;
const byte segPins[]={0,1,5,4,11,12,13,A3};
const float r1Val[]={1000.0,10000.0,100000.0,910000.0};
const float vR1=98200.0, vR2=9600.0, vDiode=0.568, shunt=4.2, vRef=4.77;
const byte segDig[10] PROGMEM={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};

byte state=0, mode=0, expIdx=0, rngIdx=0, instStep=0, expSection=0;
byte logicIn1=pD4, logicIn2=pD5, logicOut=pD6;
int paramVal=0;
float lastV=-999, lastI=-999, lastR=-999, zeroOff=0;
unsigned long lastUpd=0, lastBtn=0;
bool lcdOff=false;

#define ST_MENU 0
#define ST_OHM_RNG 1
#define ST_OHM_M 2
#define ST_VOLT 3
#define ST_AMP 4
#define ST_CONT 5
#define ST_EXP_SEC 6
#define ST_EXP_MENU 7
#define ST_EXP_INST 8
#define ST_EXP_PARAM 9
#define ST_EXP_CONF 10
#define ST_EXP_RUN 11
#define ST_ABOUT 12

void setup(){
  Wire.begin(); lcd.init(); lcd.backlight();
  pinMode(decA0,OUTPUT); pinMode(decA1,OUTPUT);
  pinMode(btnR,INPUT); pinMode(btnL,INPUT); pinMode(btnOK,INPUT); pinMode(btnB,INPUT);
  pinMode(sysBuz,OUTPUT); pinMode(pD2,OUTPUT); pinMode(pD3,OUTPUT);
  pinMode(pD4,OUTPUT); pinMode(pD5,OUTPUT); pinMode(pD6,OUTPUT);
  lcd.setCursor(3,0); lcd.print(F("MULTIMETER"));
  lcd.setCursor(5,1); lcd.print(F("v4.1")); delay(2000); showMenu();
}

void loop(){
  bool bR=chkBtn(btnR), bL=chkBtn(btnL), bOK=chkBtn(btnOK), bB=chkBtn(btnB);
  switch(state){
    case ST_MENU: doMenu(bR,bL,bOK); break;
    case ST_OHM_RNG: doOhmRng(bR,bL,bOK,bB); break;
    case ST_OHM_M: if(bB){state=ST_OHM_RNG; showRng();} else measR(); break;
    case ST_VOLT: if(bB) goMenu(); else measV(); break;
    case ST_AMP: if(bB) goMenu(); else measI(); break;
    case ST_CONT: if(bB) goMenu(); else measCont(); break;
    case ST_EXP_SEC: doExpSection(bR,bL,bOK,bB); break;
    case ST_EXP_MENU: doExpMenu(bR,bL,bOK,bB); break;
    case ST_EXP_INST: doExpInst(bOK,bB); break;
    case ST_EXP_PARAM: doExpParam(bR,bL,bOK,bB); break;
    case ST_EXP_CONF: doExpConf(bOK,bB); break;
    case ST_EXP_RUN: doExpRun(bB); break;
    case ST_ABOUT: doAbout(); break;
  }
  delay(10);
}

bool chkBtn(byte pin){
  static bool lastState[4]={LOW,LOW,LOW,LOW};
  byte idx=(pin==btnR)?0:(pin==btnL)?1:(pin==btnOK)?2:3;
  bool cur=digitalRead(pin);
  if(cur==HIGH && lastState[idx]==LOW && millis()-lastBtn>250){
    lastBtn=millis(); lastState[idx]=HIGH; return true;
  }
  if(cur==LOW && lastState[idx]==HIGH) lastState[idx]=LOW;
  return false;
}

void waitBtn(){
  delay(100);
  while(digitalRead(btnR)||digitalRead(btnL)||digitalRead(btnOK)||digitalRead(btnB)) delay(20);
  delay(150);
}

void goMenu(){
  digitalWrite(sysBuz,LOW); rstPins(); state=ST_MENU; mode=0; showMenu(); waitBtn();
}

void goExp(){
  if(lcdOff) resumeLcd();
  rstPins(); digitalWrite(sysBuz,LOW); state=ST_EXP_SEC; expSection=0; showExpSection(); waitBtn();
}

void rstPins(){
  digitalWrite(pD2,LOW); digitalWrite(pD3,LOW); digitalWrite(pD4,LOW);
  digitalWrite(pD5,LOW); digitalWrite(pD6,LOW); digitalWrite(pA0,LOW);
}

void pauseLcd(){
  lcd.clear(); lcd.print(F("Good Luck!")); lcd.setCursor(0,1); lcd.print(F("BACK to exit"));
  delay(100); lcdOff=true; pinMode(pD0,OUTPUT); pinMode(pD1,OUTPUT);
}

void resumeLcd(){
  digitalWrite(pD0,LOW); digitalWrite(pD1,LOW); pinMode(pD0,INPUT); pinMode(pD1,INPUT);
  delay(150); lcd.init(); lcd.backlight(); lcdOff=false;
}

void showMenu(){
  lcd.clear(); lcd.print(F("==MAIN MENU=="));
  lcd.setCursor(0,1); lcd.print(F("<"));
  switch(mode){
    case 0: lcd.print(F("Ohmmeter   ")); break;
    case 1: lcd.print(F("Voltmeter  ")); break;
    case 2: lcd.print(F("Ammeter    ")); break;
    case 3: lcd.print(F("Continuity ")); break;
    case 4: lcd.print(F("Experiments")); break;
    case 5: lcd.print(F("About Us   ")); break;
  }
  lcd.print(F(">"));
}

void doMenu(bool r, bool l, bool ok){
  if(r){mode=(mode+1)%6; showMenu();}
  if(l){mode=(mode+5)%6; showMenu();}
  if(ok){
    lcd.clear();
    switch(mode){
      case 0: lcd.print(F("OHMMETER")); delay(800); state=ST_OHM_RNG; showRng(); break;
      case 1: lcd.print(F("VOLTMETER")); delay(800); lcd.clear(); lastV=-999; state=ST_VOLT; break;
      case 2: lcd.print(F("AMMETER")); lcd.setCursor(0,1); lcd.print(F("Calibrating...")); calibI(); delay(500); lcd.clear(); lastI=-999; state=ST_AMP; break;
      case 3: lcd.print(F("CONTINUITY")); delay(800); lcd.clear(); state=ST_CONT; break;
      case 4: lcd.print(F("EXPERIMENTS")); delay(800); expSection=0; expIdx=0; state=ST_EXP_SEC; showExpSection(); break;
      case 5: state=ST_ABOUT; break;
    }
    waitBtn();
  }
}

void showRng(){
  lcd.clear(); lcd.print(F("Select Range:")); lcd.setCursor(0,1); lcd.print(F("< "));
  switch(rngIdx){
    case 0: lcd.print(F("5k")); break;
    case 1: lcd.print(F("100k")); break;
    case 2: lcd.print(F("500k")); break;
    case 3: lcd.print(F("5M")); break;
  }
  lcd.print(F(" Ohm >"));
}

void doOhmRng(bool r, bool l, bool ok, bool b){
  if(r){rngIdx=(rngIdx+1)%4; showRng();}
  if(l){rngIdx=(rngIdx+3)%4; showRng();}
  if(ok){lcd.clear(); lcd.print(F("Range Locked!")); delay(1000); lcd.clear(); lastR=-999; state=ST_OHM_M; waitBtn();}
  if(b) goMenu();
}

void setDec(byte r){digitalWrite(decA0,r&1); digitalWrite(decA1,(r>>1)&1); delay(30);}

void measR(){
  if(millis()-lastUpd<500) return;
  setDec(rngIdx);
  long t=0; for(byte i=0; i<10; i++){t+=analogRead(aOhm); delay(5);}
  int adc=t/10;
  float vo=adc*(vRef/1023.0), r2=(vo*r1Val[rngIdx])/(vRef-vo);
  if(abs(r2-lastR)>lastR*0.03 || lastR<0){
    lcd.setCursor(0,0); lcd.print(F("                ")); lcd.setCursor(0,0);
    if(vo>4.5||r2<0) lcd.print(F("R: Open Circuit"));
    else{lcd.print(F("R: ")); printR(r2);}
    lcd.setCursor(0,1); lcd.print(F("Range: "));
    switch(rngIdx){
      case 0: lcd.print(F("5k  ")); break;
      case 1: lcd.print(F("100k ")); break;
      case 2: lcd.print(F("500k")); break;
      case 3: lcd.print(F("5M")); break;
    }
    lastR=r2;
  }
  lastUpd=millis();
}

void printR(float r){
  if(r<1000){lcd.print((int)r); lcd.print(F(" Ohm"));}
  else if(r<1000000){lcd.print(r/1000.0,1); lcd.print(F(" kOhm"));}
  else{lcd.print(r/1000000.0,2); lcd.print(F(" MOhm"));}
}

void measV(){
  if(millis()-lastUpd<500) return;
  long t=0; for(byte i=0; i<10; i++){t+=analogRead(aVolt); delay(5);}
  float vo=(t/10)*(vRef/1023.0), vi=(vo>0.08)?vo*(1.0+vR1/vR2)+vDiode:0;
  if(abs(vi-lastV)>0.05||lastV<0){
    lcd.setCursor(0,0); lcd.print(F("==VOLTAGE==     "));
    lcd.setCursor(0,1); lcd.print(F("    ")); lcd.print(vi,2); lcd.print(F(" V      "));
    lastV=vi;
  }
  lastUpd=millis();
}

void calibI(){
  long t=0; for(byte i=0; i<30; i++){t+=analogRead(aAmp); delay(10);}
  zeroOff=(t/30.0)*(vRef/1023.0); if(zeroOff>0.15) zeroOff=0;
}

void measI(){
  if(millis()-lastUpd<500) return;
  long t=0; for(byte i=0; i<15; i++){t+=analogRead(aAmp); delay(5);}
  float vd=(t/15)*(vRef/1023.0)-zeroOff; if(vd<0) vd=0;
  float mA=(vd/shunt)*1000.0; if(mA<5) mA=0;
  if(abs(mA-lastI)>2||lastI<0){
    lcd.setCursor(0,0); lcd.print(F("                ")); lcd.setCursor(0,0);
    if(mA<0.5){lcd.print(F("==CURRENT==")); lcd.setCursor(0,1); lcd.print(F("    0.0 mA      ")); digitalWrite(sysBuz,LOW);}
    else if(mA>200){lcd.print(F("!OVERCURRENT!")); lcd.setCursor(0,1); lcd.print(F(">200mA DANGER!  ")); digitalWrite(sysBuz,HIGH); delay(100); digitalWrite(sysBuz,LOW);}
    else{lcd.print(F("==CURRENT==")); lcd.setCursor(0,1); lcd.print(F("    ")); lcd.print(mA,1); lcd.print(F(" mA      ")); digitalWrite(sysBuz,LOW);}
    lastI=mA;
  }
  lastUpd=millis();
}

void measCont(){
  if(millis()-lastUpd<300) return;
  setDec(0);
  long t=0; for(byte i=0; i<10; i++){t+=analogRead(aOhm); delay(5);}
  float vo=(t/10)*(vRef/1023.0), r2=(vo*r1Val[0])/(vRef-vo);
  lcd.clear();
  if(vo>4.5||r2<0){lcd.print(F("  NO CONTACT")); lcd.setCursor(0,1); lcd.print(F("  Open Circuit")); digitalWrite(sysBuz,LOW);}
  else if(r2<100){lcd.print(F(" CONTINUITY OK")); lcd.setCursor(0,1); lcd.print(F("R: ")); printR(r2); digitalWrite(sysBuz,HIGH);}
  else{lcd.print(F("   NO BEEP")); lcd.setCursor(0,1); lcd.print(F("R: ")); printR(r2); digitalWrite(sysBuz,LOW);}
  lastUpd=millis();
}

void showExpSection(){
  lcd.clear(); lcd.print(F("EXPERIMENTS"));
  lcd.setCursor(0,1); lcd.print(F("<"));
  lcd.print(expSection==0?F("Electronics  "):F("Digital Logic"));
  lcd.print(F(">"));
}

void doExpSection(bool r, bool l, bool ok, bool b){
  if(r||l){expSection=1-expSection; showExpSection();}
  if(ok){expIdx=0; state=ST_EXP_MENU; showExpMenu(); waitBtn();}
  if(b) goMenu();
}

void showExpMenu(){
  lcd.clear();
  if(expSection==0){
    lcd.print(F("ELECTRONICS ")); lcd.print(expIdx+1); lcd.print(F("/8"));
    lcd.setCursor(0,1); lcd.print(F("<"));
    switch(expIdx){
      case 0: lcd.print(F("LED Blink   ")); break;
      case 1: lcd.print(F("LED Fade    ")); break;
      case 2: lcd.print(F("LED Scroll  ")); break;
      case 3: lcd.print(F("Servo Motor ")); break;
      case 4: lcd.print(F("IR Sensor   ")); break;
      case 5: lcd.print(F("LDR Sensor  ")); break;
      case 6: lcd.print(F("7-Segment   ")); break;
      case 7: lcd.print(F("Buzzer      ")); break;
    }
  }else{
    lcd.print(F("LOGIC ")); lcd.print(expIdx+1); lcd.print(F("/7"));
    lcd.setCursor(0,1); lcd.print(F("<"));
    switch(expIdx){
      case 0: lcd.print(F("AND Gate    ")); break;
      case 1: lcd.print(F("OR Gate     ")); break;
      case 2: lcd.print(F("NOT Gate    ")); break;
      case 3: lcd.print(F("NAND Gate   ")); break;
      case 4: lcd.print(F("NOR Gate    ")); break;
      case 5: lcd.print(F("XOR Gate    ")); break;
      case 6: lcd.print(F("XNOR Gate   ")); break;
    }
  }
  lcd.print(F(">"));
}

void doExpMenu(bool r, bool l, bool ok, bool b){
  byte maxExp=(expSection==0)?8:7;
  if(r){expIdx=(expIdx+1)%maxExp; showExpMenu();}
  if(l){expIdx=(expIdx+maxExp-1)%maxExp; showExpMenu();}
  if(ok){instStep=0; state=ST_EXP_INST; showInst(); waitBtn();}
  if(b){state=ST_EXP_SEC; showExpSection(); waitBtn();}
}

byte getInstCnt(){
  if(expSection==0){if(expIdx==2) return 4; if(expIdx==6) return 8; return 1;}
  return (expIdx==2)?1:2;
}

void showInst(){
  lcd.clear(); lcd.print(F("Step ")); lcd.print(instStep+1); lcd.print(F("/")); lcd.print(getInstCnt());
  lcd.setCursor(0,1);
  if(expSection==0){
    switch(expIdx){
      case 0: case 1: case 7: lcd.print(F("d3->Component")); break;
      case 2:
        switch(instStep){
          case 0: lcd.print(F("d3->LED1 anode")); break;
          case 1: lcd.print(F("d2->LED2 anode")); break;
          case 2: lcd.print(F("d4->LED3 anode")); break;
          case 3: lcd.print(F("Cathodes->GND")); break;
        }
        break;
      case 3: lcd.print(F("a0->Servo sig")); break;
      case 4: lcd.print(F("a0->IR OUT pin")); break;
      case 5: lcd.print(F("a0->LDR+10k")); break;
      case 6:
        switch(instStep){
          case 0: lcd.print(F("d0->Seg A")); break;
          case 1: lcd.print(F("d1->Seg B")); break;
          case 2: lcd.print(F("d3->Seg C")); break;
          case 3: lcd.print(F("d2->Seg D")); break;
          case 4: lcd.print(F("d4->Seg E")); break;
          case 5: lcd.print(F("d5->Seg F")); break;
          case 6: lcd.print(F("d6->Seg G")); break;
          case 7: lcd.print(F("a0->Seg DP")); break;
        }
        break;
    }
  }else{
    if(expIdx==2) lcd.print(F("BTN:d4 LED:d6"));
    else{
      if(instStep==0) lcd.print(F("BTN1:d4 BTN2:d5"));
      else lcd.print(F("LED:d6+resistor"));
    }
  }
}

void doExpInst(bool ok, bool b){
  if(ok){
    if(instStep<getInstCnt()-1){instStep++; showInst();}
    else{
      if(expSection==0 && (expIdx==0||expIdx==3)){state=ST_EXP_PARAM; paramVal=(expIdx==0)?500:90; showParam();}
      else{state=ST_EXP_CONF; showConf();}
    }
    waitBtn();
  }
  if(b){state=ST_EXP_MENU; showExpMenu(); waitBtn();}
}

void showParam(){
  lcd.clear();
  if(expIdx==0){lcd.print(F("Blink Delay:")); lcd.setCursor(0,1); lcd.print(F("<-   ")); lcd.print(paramVal/1000.0,1); lcd.print(F("s   +>"));}
  else{lcd.print(F("Servo Angle:")); lcd.setCursor(0,1); lcd.print(F("<-   ")); lcd.print(paramVal); lcd.print((char)223); lcd.print(F("   +>"));}
}

void doExpParam(bool r, bool l, bool ok, bool b){
  byte step=(expIdx==0)?100:10; int maxV=(expIdx==0)?2000:180, minV=(expIdx==0)?100:0;
  if(r && paramVal<maxV){paramVal+=step; showParam();}
  if(l && paramVal>minV){paramVal-=step; showParam();}
  if(ok){state=ST_EXP_CONF; showConf(); waitBtn();}
  if(b){instStep=0; state=ST_EXP_INST; showInst(); waitBtn();}
}

void showConf(){
  lcd.clear(); lcd.print(F("Ready?")); lcd.setCursor(0,1); lcd.print(F("OK=Start BACK=No"));
}

void doExpConf(bool ok, bool b){
  if(ok){state=ST_EXP_RUN; startExp(); waitBtn();}
  if(b){
    if(expSection==0 && (expIdx==0||expIdx==3)){state=ST_EXP_PARAM; showParam();}
    else{instStep=0; state=ST_EXP_INST; showInst();}
    waitBtn();
  }
}

void startExp(){
  if(expSection==0 && expIdx==6){
    pauseLcd(); for(byte i=0; i<8; i++){pinMode(segPins[i],OUTPUT); digitalWrite(segPins[i],LOW);}
  }else{
    lcd.clear(); lcd.print(F("Good Luck!")); delay(1000); setupExp();
  }
}

void setupExp(){
  rstPins();
  if(expSection==0){
    switch(expIdx){
      case 0: case 1: case 7: pinMode(pD2,OUTPUT); break;
      case 2: pinMode(pD2,OUTPUT); pinMode(pD3,OUTPUT); pinMode(pD4,OUTPUT); break;
      case 3: myServo.attach(pA0); break;
      case 4: case 5: pinMode(pA0,INPUT); lcd.clear(); break;
    }
  }else{
    pinMode(logicIn1,INPUT); if(expIdx!=2) pinMode(logicIn2,INPUT);
    pinMode(logicOut,OUTPUT); digitalWrite(logicOut,LOW); lcd.clear();
  }
}

void doExpRun(bool b){
  if(b){stopExp(); return;}
  runExp();
}

void stopExp(){
  if(expSection==0){
    if(expIdx==6) for(byte i=0; i<8; i++) digitalWrite(segPins[i],LOW);
    if(expIdx==3) myServo.detach();
    if(expIdx==7) noTone(pD2);
  }else digitalWrite(logicOut,LOW);
  goExp();
}

void runExp(){
  if(expSection==0){
    switch(expIdx){
      case 0: runBlink(); break;
      case 1: runFade(); break;
      case 2: runScroll(); break;
      case 3: runServo(); break;
      case 4: runIR(); break;
      case 5: runLDR(); break;
      case 6: run7Seg(); break;
      case 7: runBuz(); break;
    }
  }else runLogicGate();
}

void runBlink(){
  static unsigned long lt=0; static bool st=false;
  if(millis()-lt>(unsigned long)paramVal){st=!st; digitalWrite(pD2,st); lt=millis();}
}

void runFade(){
  static int b=0, s=3; static unsigned long lu=0;
  if(millis()-lu>=15){analogWrite(pD2,b); b+=s;
    if(b>=255){b=255; s=-3;} else if(b<=0){b=0; s=3;} lu=millis();}
}

void runScroll(){
  static byte cur=0; static unsigned long lt=0;
  if(millis()-lt>300){digitalWrite(pD2,cur==0); digitalWrite(pD3,cur==1); digitalWrite(pD4,cur==2); cur=(cur+1)%3; lt=millis();}
}

void runServo(){
  static int ang=0, dir=1; static unsigned long lm=0; static bool init=false;
  if(!init){lcd.clear(); lcd.print(F("Angle: ")); ang=paramVal; myServo.write(ang); init=true; delay(500);}
  if(millis()-lm>=20){ang+=dir; if(ang>=180){ang=180; dir=-1;} else if(ang<=0){ang=0; dir=1;}
    myServo.write(ang); lcd.setCursor(7,0); lcd.print(F("   ")); lcd.setCursor(7,0); lcd.print(ang); lcd.print((char)223); lm=millis();}
}

void runIR(){
  static unsigned long lt=0; static bool last=false, first=true;
  if(first){lcd.clear(); lcd.print(F("IR Sensor")); first=false;}
  if(millis()-lt>200){bool det=(analogRead(pA0)<512);
    if(det!=last){lcd.setCursor(0,1); lcd.print(det?F("Object Detected!"):F("No Object       ")); last=det;} lt=millis();}
}

void runLDR(){
  static unsigned long lu=0; static int lp=-1; static bool first=true;
  if(first){lcd.clear(); lcd.setCursor(0,0); lcd.print(F("LDR Sensor")); first=false;}
  if(millis()-lu>=400){long t=0; for(byte i=0; i<10; i++){t+=analogRead(pA0); delay(2);} int adc=t/10;
    float vA0=(adc/1023.0)*vRef, rLdr;
    if(vA0>0.1) rLdr=10000.0*((vRef-vA0)/vA0); else rLdr=999999;
    rLdr=constrain(rLdr,100,999999);
    int lp2; if(rLdr<1000) lp2=100; else if(rLdr>100000) lp2=0; else lp2=map(rLdr,1000,100000,100,0);
    if(abs(lp2-lp)>1||lp<0){
      lcd.setCursor(0,1); lcd.print(F("                ")); lcd.setCursor(0,1);
      lcd.print(F("Light:")); lcd.print(lp2); lcd.print(F("% R:"));
      if(rLdr<1000){lcd.print((int)rLdr); lcd.print(F("R"));}
      else if(rLdr<100000){lcd.print(rLdr/1000.0,1); lcd.print(F("k"));}
      else lcd.print(F(">100k"));
      lp=lp2;
    }
    lu=millis();
  }
}

void run7Seg(){
  static byte dig=0; static unsigned long lt=0;
  if(millis()-lt>1000){byte pat=pgm_read_byte(&segDig[dig]);
    for(byte i=0; i<7; i++) digitalWrite(segPins[i],(pat>>i)&1);
    dig=(dig+1)%10; lt=millis();}
}

void runBuz(){
  static byte pat=0; static unsigned long lt=0, pt=0;
  if(!lcdOff){lcd.setCursor(0,0); lcd.print(F("Buzzer Pattern ")); lcd.print(pat+1);}
  unsigned long el=millis()-pt;
  switch(pat){
    case 0: tone(pD2,(el/200)%2?0:1000); if((el/200)%2) noTone(pD2); break;
    case 1: tone(pD2,500+(el%1000)); break;
    case 2: tone(pD2,(el/100)%2?2000:1500); break;
    case 3: {int n[]={262,294,330,349,392}; tone(pD2,n[(el/300)%5]);} break;
    case 4: {int c=el%1000; if(c<100||(c>200&&c<300)) tone(pD2,500); else noTone(pD2);} break;
  }
  if(millis()-lt>3000){noTone(pD2); pat=(pat+1)%5; pt=millis(); lt=millis(); delay(200);}
}

// ========== DIGITAL LOGIC GATES ==========
/*
 DIGITAL ELECTRONICS EXPERIMENTS
 
 For all logic gates:
 - Connect push button 1 to pin d4 (Input A)
 - Connect push button 2 to pin d5 (Input B) - except NOT gate
 - Connect LED with resistor (220Ω) to pin d6 (Output)
 - Connect other ends of buttons to GND
 - Connect LED cathode to GND
 
 TRUTH CONDITIONS:
 - AND: LED ON only when BOTH buttons pressed
 - OR: LED ON when ANY button pressed
 - NOT: LED ON when button NOT pressed
 - NAND: LED OFF only when BOTH buttons pressed
 - NOR: LED OFF when ANY button pressed
 - XOR: LED ON when buttons in DIFFERENT states
 - XNOR: LED ON when buttons in SAME state
*/

void runLogicGate(){
  static unsigned long lu=0; static bool last=false;
  if(millis()-lu<100) return;
  
  bool in1=digitalRead(logicIn1);
  bool in2=(expIdx==2)?false:digitalRead(logicIn2);
  bool out=false;
  
  switch(expIdx){
    case 0: out=in1 && in2; break;        // AND
    case 1: out=in1 || in2; break;        // OR
    case 2: out=!in1; break;              // NOT
    case 3: out=!(in1 && in2); break;     // NAND
    case 4: out=!(in1 || in2); break;     // NOR
    case 5: out=in1 != in2; break;        // XOR
    case 6: out=in1 == in2; break;        // XNOR
  }
  
  digitalWrite(logicOut,out);
  
  if(out!=last){
    lcd.setCursor(0,0);
    if(expIdx==2){
      lcd.print(F("IN:")); lcd.print(in1?F("1"):F("0"));
      lcd.print(F(" OUT:")); lcd.print(out?F("1"):F("0")); lcd.print(F("   "));
    }else{
      lcd.print(F("A:")); lcd.print(in1?F("1"):F("0"));
      lcd.print(F(" B:")); lcd.print(in2?F("1"):F("0"));
      lcd.print(F(" OUT:")); lcd.print(out?F("1"):F("0")); lcd.print(F(" "));
    }
    lcd.setCursor(0,1);
    lcd.print(F("LED:")); lcd.print(out?F("ON "):F("OFF")); lcd.print(F("        "));
    last=out;
  }
  lu=millis();
}

void doAbout(){
  lcd.clear(); lcd.setCursor(2,0); lcd.print(F("We proudly"));
  lcd.setCursor(3,1); lcd.print(F("present...")); delay(3000);
  lcd.clear(); lcd.print(F("================"));
  lcd.setCursor(2,1); lcd.print(F("TRAINER KIT")); delay(3000);
  lcd.clear(); lcd.setCursor(4,0); lcd.print(F("Made by")); delay(3000);
  lcd.clear(); lcd.setCursor(0,0); lcd.print(F("Adnan, Abrar &"));
  lcd.setCursor(4,1); lcd.print(F("Owaize")); delay(5000);
  goMenu();
}
