#include <Wire.h>
//#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

//键值
#define KOGAN_POWER 0x20B
#define KOGAN_INPUT 0x21C
#define KOGAN_UP 0x22B
#define KOGAN_DOWN 0x22C
#define KOGAN_LEFT 0x22D
#define KOGAN_RIGHT 0x22E
#define KOGAN_DTV 0x232
#define KOGAN_MENU 0x215
#define KOGAN_ENTER 0x22F
#define KOGAN_ASPECT 0x231
#define KOGAN_EXIT 0x230
#define KOGAN_3D 0x233

IRsend irsend;

//LiquidCrystal_I2C lcd(0x20, 16, 2); //设置 LCD 的地址为 0x20,可以设置 2行,每行 16 个字符


unsigned long dataPin0 = 12;

unsigned int EchoPin = 9; //将 Arduino 的 Pin2 连接至 US-100 的 Echo/RX
unsigned int TrigPin = 8; //将 Arduino 的 Pin3 连接至 US-100 的 Trig/TX

int pushButton1 = 8;  //大按钮红色
int pushButton2 = 7;  //大按钮黄色

int speakerPin = 4;  //buzzer auto
int ledPin = 2; //提示灯

float angleX = 0;
float angleY = 0;
int servoAngleX = 0;
int servoAngleY = 0;
int servoXMiddleValue = 0;


enum {
  TASK_IDLE = 0,
  TASK_FTV = 1,  //finding TV
  TASK_SWITH = 2,  //switch to HDMI
};

char task_status_string[3][20] = {
  "TASK_IDLE",
  "TASK_FTV",
  "TASK_SWITH",
};

int task_status;

enum {
  RET_FAIL = 0,   // error, for example timeout
  RET_NOT_FOUND = 1,  //not found
  RET_OK = 2,   //found
  RET_UP = 3,   //requires UP
  RET_DOWN = 4,  //requires DOWN
};

char ret_code_string[5][20] = {
  "RET_FAIL",
  "RET_NOT_FOUND",
  "RET_OK",
  "RET_UP",
  "RET_DOWN"
};

int ret_code;

//默认设为初始位置
int centerX = 90;
int centerY = 90;

//I2C下达的指令
// '1' to find TV, '2' to switch to HDMI
// '0' means idle
char charCmdByI2C = '0';

int blinkpin = 13;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


void setup()
{
  //Initialize
  pinMode(blinkpin, OUTPUT);  //I2C接收闪烁

  Wire.begin(8);                // join i2c bus with address #8
  Wire.onReceive(receiveEventI2C); // register event

  Serial.begin(57600);
  Serial.print("Start");

  pinMode(pushButton1, INPUT);
  pinMode(pushButton2, INPUT);
  //  pinMode(buzzerPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);

  pinMode(ledPin, OUTPUT);

  //  lcd.init();
  //  lcd.backlight();
  //  lcd.print("Waiting for BLE"); //LCD屏显示
  //  lcd.setCursor(0, 1); //光标移到第2行,第一个字符
  //  lcd.print("..."); //LCD屏显示”Temp(C):”

  digitalWrite(speakerPin, HIGH); delay(500); digitalWrite(speakerPin, LOW);

  servo_init();

  pinMode(dataPin0, OUTPUT);
  digitalWrite(dataPin0, HIGH);

  digitalWrite(ledPin, HIGH);
  delay(500);
  digitalWrite(ledPin, LOW);
}

int buttonState1 = 0;
int buttonState2 = 0;
int prevButtonState1 = 0;
int prevButtonState2 = 0;

int XaxisPin = A1;
int YaxisPin = A0;
int ZswPin = A2;
int valueX, valueY, valueZ;

#define CMD_COUNT_THRES 3
int cmdCount = 0;

// Blocks until another byte is available on serial port
char readChar()
{
  while (Serial.available() < 1) { } // Block
  return Serial.read();
}


float getAngle()
{
  byte id[5]; //e.g. id = "$+0062" mean 6.2
  id[0] = readChar();
  id[1] = readChar() - '0';
  id[2] = readChar() - '0';
  id[3] = readChar() - '0';
  id[4] = readChar() - '0';
  float angle = (id[0] == '+' ? 1.0 : -1.0) * (id[1] * 100.0 + id[2] * 10.0 + id[3] + id[4] * 0.1);
  return angle;
}

void do_parse_command()
{
  int incomingByte = Serial.available();
  if (incomingByte > 0)
  {
    int val = Serial.read();
    if (val == '!')  //for test
    {
      //      lcd.setCursor(0, 0);
      //      lcd.print("BLE ECHO TEST");
      Serial.println("ECHO: <!>");
    }
  }
}

//等待EYE反馈并解析结果
int do_parse_reply()
{
  int count = 50; //等5s,无响应则返回
  int incomingByte;
  int ret_code = RET_FAIL;

  while (count > 0)
  {
    incomingByte = Serial.available();
    if (incomingByte > 0)
    {
      int val = Serial.read();
      if (val == '~') //命令反馈
      {
        byte rpt; //e.g.  "~N" means not found, "~O" means OK
        rpt = readChar();

        Serial.print("Recv:~"); Serial.println((char)rpt);
        switch (rpt)
        {
          case 'N':
            ret_code = RET_NOT_FOUND;
            break;
          case 'O':
            ret_code = RET_OK;
            break;
          case 'U':
            ret_code = RET_UP;
            break;
          case 'D':
            ret_code = RET_DOWN;
            break;
          case 'F':
          default:
            ret_code = RET_FAIL;
            break;
        }
        return ret_code;
      }
      else if (val == '!')  //for test
      {
        //        lcd.setCursor(0, 0);
        //        lcd.print("BLE ECHO TEST");
        Serial.println("ECHO: <!>");
      }
    }
    delay(100);
    count--;
  }
  ret_code = RET_NOT_FOUND;   //调试
  return ret_code;
}

void toFindTV()
{
  digitalWrite(speakerPin, HIGH); delay(100); digitalWrite(speakerPin, LOW);
  //    lcd.setCursor(1, 1);
  //    lcd.println("FINDING TV MODE");

  if (task_status == TASK_IDLE)
  {
    bool ret = do_finding_TV(); //开始,独占模式
    Serial.print("result="); Serial.println(ret);
    if (ret)
    {
      digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
      delay(100);
      digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
    }
    else
    {
      digitalWrite(speakerPin, HIGH); delay(500); digitalWrite(speakerPin, LOW);
    }
  }
  delay(100);
}

void toSwitchHDMI()
{
  digitalWrite(speakerPin, HIGH); delay(100); digitalWrite(speakerPin, LOW);
  //    lcd.setCursor(1, 1);
  //    lcd.println("FINDING TV MODE");

  if (task_status == TASK_IDLE)
  {
    bool ret = do_switch_HDMI(); //开始,独占模式
    Serial.print("result="); Serial.println(ret);
    if (ret)
    {
      digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
      delay(100);
      digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
    }
    else
    {
      digitalWrite(speakerPin, HIGH); delay(500); digitalWrite(speakerPin, LOW);
    }
  }
  delay(100);
}



bool toTestRemote()
{
  for (int i = 0; i < 3; i++)
  {
    irsend.sendKogan(KOGAN_EXIT, 32);
    delay(200);
    irsend.sendKogan(KOGAN_MENU, 32);
    digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
    delay(500);
    irsend.sendKogan(KOGAN_DOWN, 32);
    delay(500);
    irsend.sendKogan(KOGAN_DOWN, 32);
    delay(500);
  }

  irsend.sendKogan(KOGAN_EXIT, 32);
  //over
  digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
  delay(100);
  digitalWrite(speakerPin, HIGH); delay(50); digitalWrite(speakerPin, LOW);
}


////////////////////////////////////////////////////////
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

unsigned long currentMillis, previousMillis;
int count = 0;
bool I2C_CMD_INDICATOR = false;
void loop()
{
  if (I2C_CMD_INDICATOR)
  {
    Serial.println("I2C_CMD_INDICATOR");
    for (int i = 0; i < 2; i++)
    {
      digitalWrite(ledPin, HIGH);
      digitalWrite(speakerPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      digitalWrite(speakerPin, LOW);
      delay(100);
    }
    I2C_CMD_INDICATOR = false;
  }

  valueX = analogRead(XaxisPin); //读取模拟端口0
  valueY = analogRead(YaxisPin); //读取模拟端口1
  valueZ = analogRead(ZswPin); //读取模拟端口2

  //轮询Slave内容
  currentMillis = millis();
  if (currentMillis - previousMillis > 1000) //2s
  {
    previousMillis = currentMillis;
  }

  do_parse_command(); //现在只用于蓝牙echo

  //    Serial.print("X:"); Serial.print(valueX, DEC);
  //    Serial.print(" | Y:"); Serial.print(valueY, DEC);
  //    Serial.print(" | Z: "); Serial.println(valueZ, DEC);

  if (valueZ == 0 && abs(valueX - 500) < 50 && abs(valueY - 500) < 50 )
  {
    servo_reset();
  }
  else
  {
    int stepX = abs(valueX - 500) / 100;
    if (valueX > 550)
      servo_left(stepX);
    else if (valueX < 450)
      servo_right(stepX);

    int stepY = abs(valueY - 500) / 100;
    if (valueY > 550)
      servo_up(stepY);
    else if (valueY < 450)
      servo_down(stepY);
  }

  //按下按钮,开始功能
  buttonState1 = digitalRead(pushButton1);
  if (buttonState1 == 1 && prevButtonState1 == 0) //pushed
  {
    toFindTV();
  }
  prevButtonState1 = buttonState1;


  buttonState2 = digitalRead(pushButton2);
  if (buttonState2 == 1 && prevButtonState2 == 0) //pushed
  {
    toSwitchHDMI();
  }
  prevButtonState2 = buttonState2;
  //  delay(500); //每秒(1000ms)测量一次

  switch (charCmdByI2C)
  {
    case '1': //to find TV
      {
        if (task_status == TASK_IDLE)
        {
          toFindTV();
        }
        charCmdByI2C = '0';
        break;
      }

    case '2': //to switch to HDMI
      {
        if (task_status == TASK_IDLE)
        {
          toSwitchHDMI();
        }
        charCmdByI2C = '0';
        break;
      }

    case '3': //to test remote
      {
        if (task_status == TASK_IDLE)
        {
          toTestRemote();
        }
        charCmdByI2C = '0';
        break;
      }

    case 'b': //to beep
      {
        if (task_status == TASK_IDLE)
        {
          digitalWrite(speakerPin, HIGH); delay(50); 
          digitalWrite(speakerPin, LOW); delay(50); 
        }
        charCmdByI2C = '0';
        break;
      }
    case '0': //idle
    default:
      break;
  }

}


bool do_finding_TV()
{
  //进入此状态,移动到初始位置,告知EYE寻找Menu,等待结果
  task_status = TASK_FTV;

  int positionX = 50;
  int positionY = 90;
  //Servo扫描就绪
  servo_scan_reset(positionX, positionY);

  //发射IR, 显示Menu
  irsend.sendKogan(KOGAN_EXIT, 32);
  delay(200);
  irsend.sendKogan(KOGAN_MENU, 32);

  Serial.println("finding TV...");
  while (1)
  {
    //    Serial.print("Servo is X="); Serial.print(getServoX());
    //    Serial.print(", Y="); Serial.println(getServoY());

    //发出命令字, "~"开头
    Serial.println("~MENU");  //要求EYE寻找MENU,不得带有KONKA_字样
    int ret_code = do_parse_reply(); //等待回复,blocking
    Serial.print("RET:"); Serial.println(ret_code_string[ret_code]);

    if (ret_code == RET_OK)
    {
      centerX = positionX;
      centerY = positionY;
      task_status = TASK_IDLE;
      return true;
    }
    else if (ret_code == RET_NOT_FOUND)
    {
      if (positionX >= 115) //找完一轮,以失败告终
      {
        task_status = TASK_IDLE;
        return false;
      }
      else
      {
        if (positionY > 85) {
          positionY -= 10;
        }
        else
        {
          positionX += 20;
          positionY = 90;
        }
        servo_scan_reset(positionX, positionY);
        //再次显示Menu并移动舵机
        irsend.sendKogan(KOGAN_EXIT, 32);
        delay(100);
        irsend.sendKogan(KOGAN_MENU, 32);
      }
    }
    else
    {
      task_status = TASK_IDLE;
      return false;
    }

  }
}

//左右摇动
int server_x_shift[5] = {0, -4, -2, +4, +2};
int index_shift = 0;

bool do_switch_HDMI()
{
  //进入此状态,移动到初始位置,告知EYE寻找Menu,等待结果
  task_status = TASK_SWITH;

  //Servo就位
  servo_scan_reset(centerX, centerY);

  //发射IR, 显示Source
  irsend.sendKogan(KOGAN_EXIT, 32);
  delay(200);
  irsend.sendKogan(KOGAN_INPUT, 32);

  Serial.println("finding HDMI...");

  unsigned long currentMs = millis();
  unsigned long previousMs = currentMs;

  index_shift = 0;
  int positionX = centerX, positionY = centerY;
  while (1)
  {
    currentMs = millis();
    if (currentMs - previousMs > 30000) //30s超时
      break;

    //发出命令字, "~"开头
    Serial.println("~KONKA_HDMI");  //要求EYE寻找KONKA_HDMI
    int ret_code = do_parse_reply(); //等待回复,blocking
    Serial.print("RET:"); Serial.println(ret_code_string[ret_code]);

    switch (ret_code)
    {
      case RET_OK:
        irsend.sendKogan(KOGAN_ENTER, 32); //进入HDMI
        task_status = TASK_IDLE;
        return true;

      case RET_NOT_FOUND:
        //发射IR, 显示Source
        irsend.sendKogan(KOGAN_EXIT, 32);
        delay(200);
        irsend.sendKogan(KOGAN_INPUT, 32);
        index_shift++;
        if (index_shift > 4)
          index_shift = 0;
        //轻微摇动
        servo_scan_reset(positionX + server_x_shift[index_shift],
                         positionY /*+ server_x_shift[index_shift]*/);
        delay(200);
        break;

      case RET_UP:
        irsend.sendKogan(KOGAN_UP, 32);
        delay(200);
        break;

      case RET_DOWN:
        irsend.sendKogan(KOGAN_DOWN, 32);
        delay(200);
        break;

      case RET_FAIL:
      default:
        task_status = TASK_IDLE;
        return false;
    }
  }

  task_status = TASK_IDLE;
  return false;
}

void receiveEventI2C(int howMany)
{
  while (Wire.available() > 0) // loop through all but the last
  {
    Serial.print("<");
    char c = Wire.read(); // receive byte as a character
    digitalWrite(blinkpin, HIGH);
    Serial.print(c); Serial.print(">--["); Serial.print((int)c); Serial.print("]");
    if (charCmdByI2C == '0')
    {
      if (c != '0')
      {
        charCmdByI2C = c;
      }
    }
  }
  Serial.println();
  I2C_CMD_INDICATOR = true;
}


