// Code from https://steemit.com/utopian-io/@lapilipinas/arduino-big-digits-0-99-with-i2c-16x2-lcd


byte LT[8] =
{
  B00111,
  B01111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};
byte UB[8] =
{
  B11111,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
byte RT[8] =
{
  B11100,
  B11110,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};
byte LL[8] =
{
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B01111,
  B00111
};
byte LB[8] =
{
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111
};
byte LR[8] =
{
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11110,
  B11100
};
byte MB[8] =
{
  B11111,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111
};
byte block[8] =
{
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

void custom0(int x){
    lcd.setCursor(x,0);
    lcd.write((byte)0); 
    lcd.write(1); 
    lcd.write(2);
    lcd.setCursor(x, 1);
    lcd.write(3); 
    lcd.write(4); 
    lcd.write(5);
  }
  void custom1(int x){
    lcd.setCursor(x,0);
    lcd.write(1);
    lcd.write(2);
    lcd.print(" ");
    lcd.setCursor(x,1);
    lcd.write(4);
    lcd.write(7);
    lcd.write(4);
  }
  void custom2(int x){
    lcd.setCursor(x,0);
    lcd.write(6);
    lcd.write(6);
    lcd.write(2);
    lcd.setCursor(x, 1);
    lcd.write(3);
    lcd.write(4);
    lcd.write(4);
  }
  void custom3(int x){
    lcd.setCursor(x,0);
    lcd.write(6);
    lcd.write(6);
    lcd.write(2);
    lcd.setCursor(x, 1);
    lcd.write(4);
    lcd.write(4);
    lcd.write(5);
  }
  void custom4(int x){
    lcd.setCursor(x,0);
    lcd.write(3);
    lcd.write(4);
    lcd.write(7);
    lcd.setCursor(x, 1);
    lcd.print(" ");
    lcd.print(" ");
    lcd.write(7);
  }
  void custom5(int x){
    lcd.setCursor(x,0);
    lcd.write(3);
    lcd.write(6);
    lcd.write(6);
    lcd.setCursor(x, 1);
    lcd.write(4);
    lcd.write(4);
    lcd.write(5);
  }
  void custom6(int x){
    lcd.setCursor(x,0);
    lcd.write((byte)0);
    lcd.write(6);
    lcd.write(6);
    lcd.setCursor(x, 1);
    lcd.write(3);
    lcd.write(4);
    lcd.write(5);
  }
  void custom7(int x){
    lcd.setCursor(x,0);
    lcd.write(1);
    lcd.write(1);
    lcd.write(2);
    lcd.setCursor(x, 1);
    lcd.print(" ");
    lcd.print(" ");
    lcd.write(7);
  }
  void custom8(int x){
    lcd.setCursor(x,0);
    lcd.write((byte)0);
    lcd.write(6);
    lcd.write(2);
    lcd.setCursor(x, 1);
    lcd.write(3);
    lcd.write(4);
    lcd.write(5);
  }
  void custom9(int x){
    lcd.setCursor(x,0);
    lcd.write((byte)0);
    lcd.write(6);
    lcd.write(2);
    lcd.setCursor(x, 1);
    lcd.print(" ");
    lcd.print(" ");
    lcd.write(7);
  }
  void printDigits(int digits, int x){
    switch (digits) {
    case 0: 
      custom0(x);
      break;
    case 1: 
      custom1(x);
      break;
    case 2: 
      custom2(x);
      break;
    case 3: 
      custom3(x);
      break;
    case 4: 
      custom4(x);
      break;
    case 5: 
      custom5(x);
      break;
    case 6: 
      custom6(x);
      break;
    case 7: 
      custom7(x);
      break;
    case 8: 
      custom8(x);
      break;
    case 9: 
      custom9(x);
      break;
    }
  }
