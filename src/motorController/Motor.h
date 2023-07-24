#ifndef MOTOR_H
#define MOTOR_H

class Motor {
  public:
    void setup();
    void forward(int speed);
    void reverse(int speed);
    void stop();
  
  private:
    int enPin = 9;
    int in1Pin = 8;
    int in2Pin = 7;
};

#endif
