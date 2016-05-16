#include <Servo.h>    //舵机控制库

//2路舵机
Servo servoX; //云台X轴舵机 左右
Servo servoY; //云台Y轴舵机 上下

int getServoX()
{
  return servoX.read();
}

int getServoY()
{
  return servoY.read();
}


//-1 means to ignore
void servo_wait_and_print(int xTarget, int yTarget)
{
  int x = 0, y = 0;
  while (1)
  {
    x = servoX.read();
    y = servoY.read();
    if ((x == xTarget || xTarget == -1) && (y == yTarget || yTarget == -1))
      break;
    delay(10);
  }
//  Serial.print("ServoX="); Serial.print(x);
//  Serial.print("        ServoY="); Serial.print(y);
//  Serial.println();

  delay(20);
}

//舵机左转
void servo_left(int step)
{
  int servotempX = servoX.read(); //获取舵机目前的角度值
  servotempX += step;
  if (servotempX <= 170 && servotempX >= 10) //我定义的舵机旋转角度为10度到170度
  {
    servoX.write(servotempX);
    delay(10);
    //    int servoCheck=servoX.read();
    //    Serial.print("ServoY real value ="); Serial.print(servoCheck); Serial.println();
  }
  else
    return;

  servo_wait_and_print(servotempX, -1);
}


//舵机右转
void servo_right(int step)
{
  int servotempX = servoX.read(); //获取舵机目前的角度值
  servotempX -= step;
  if (servotempX <= 170 && servotempX >= 10) //我定义的舵机旋转角度为10度到170度
    servoX.write(servotempX);
  else
    return;

  servo_wait_and_print(servotempX, -1);
}


//舵机上转
void servo_up(int step)
{
  int servotempY = servoY.read(); //获取舵机目前的角度值
  servotempY -= step;
  if (servotempY <= 90 && servotempY >= 10) //我定义的舵机旋转角度为10度到170度
    servoY.write(servotempY);
  else
    return;

  servo_wait_and_print(-1, servotempY);
}


//舵机下转
void servo_down(int step)
{
  int servotempY = servoY.read(); //获取舵机目前的角度值
  servotempY += step; //舵机运动1度
  if (servotempY <= 90 && servotempY >= 10) //我定义的舵机旋转角度为10度到170度
    servoY.write(servotempY);
  else
    return;

  servo_wait_and_print(-1, servotempY);
}

void servo_output_angle(int x, int y)
{
  //  Serial.println(x, y);
  if (x == 0 || y == 0)
    return;

  if (x < 10)
    x = 10;
  if (x > 170)
    x = 170;
    
  if (y < 30)
    y = 30;
  if (y > 130)
    y = 130;

  servoX.write(x);
  servoY.write(y);

  servo_wait_and_print(x, y);
}

//reset
void servo_reset()
{
  servoX.write(80);
  servoY.write(90);
  servo_wait_and_print(80, 90);
}

//reset for scanning
void servo_scan_reset(int x, int y)
{
  //分两步进行,减慢冲击
  servoX.write(x);
  int yy=getServoY();
  servo_wait_and_print(x, yy);
  
  delay(100);
  servoY.write(y);
  servo_wait_and_print(x, y);
}

void servo_init()
{
  servoX.attach(10);//水平舵机接10脚
  servoY.attach(11);//上下舵机接11脚
  servoX.write(90);//输出舵机初始位置为90度
  servoY.write(90);//输出舵机初始位置为90度

  servo_reset();
}
