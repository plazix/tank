
#include <PsxControllerHwSpi.h>
#include <DigitalIO.h>


// === Config start =======================================

#define PS2X_MOSI_PIN          11 // D11
#define PS2X_MISO_PIN          12 // D12
#define PS2X_CLK_PIN           13 // D13
#define PS2X_CS_PIN            10 // D10

#define PS2X_ANALOG_X_ZERO     128
#define PS2X_ANALOG_Y_ZERO     127

#define DRIVER_LEFT_PWM_PIN    5  // D5
#define DRIVER_LEFT_IN1_PIN    14 // A0, Left AIN1 and BIN1
#define DRIVER_LEFT_IN2_PIN    15 // A1, Left AIN2 and BIN2
#define DRIVER_LEFT_STBY_PIN   8  // D8, Left STBY

#define DRIVER_RIGHT_PWM_PIN   6  // D6
//#define DRIVER_RIGHT_IN1_PIN   16 // A2, Right AIN1 and BIN1
//#define DRIVER_RIGHT_IN2_PIN   17 // A3, Right AIN2 and BIN2
#define DRIVER_RIGHT_IN1_PIN   17 // A2, Right AIN1 and BIN1
#define DRIVER_RIGHT_IN2_PIN   16 // A3, Right AIN2 and BIN2
#define DRIVER_RIGHT_STBY_PIN  7  // D7, Right STBY

// === Config end =========================================

typedef struct {
  uint8_t pwm_pin;
  uint8_t in1_pin;
  uint8_t in2_pin;
  uint8_t stby_pin;
} driver_config_t;

typedef struct {
  byte x;
  byte y;
  bool changed;
} psx_state_analog_t;

typedef struct {
  bool detected;
  psx_state_analog_t analog_left;
  psx_state_analog_t analog_right;
} psx_state_t;

// === Global vars start =======================================

driver_config_t driver_config_left = {
  .pwm_pin = DRIVER_LEFT_PWM_PIN,
  .in1_pin = DRIVER_LEFT_IN1_PIN,
  .in2_pin = DRIVER_LEFT_IN2_PIN,
  .stby_pin = DRIVER_LEFT_STBY_PIN,
};

driver_config_t driver_config_right = {
  .pwm_pin = DRIVER_RIGHT_PWM_PIN,
  .in1_pin = DRIVER_RIGHT_IN1_PIN,
  .in2_pin = DRIVER_RIGHT_IN2_PIN,
  .stby_pin = DRIVER_RIGHT_STBY_PIN,
};

psx_state_t psx_state= {
  .detected = false,
  .analog_left = {
    .x = PS2X_ANALOG_X_ZERO,
    .y = PS2X_ANALOG_Y_ZERO,
    .changed = false,
  },
  .analog_right = {
    .x = PS2X_ANALOG_X_ZERO,
    .y = PS2X_ANALOG_Y_ZERO,
    .changed = false,
  },
};

PsxControllerHwSpi<PS2X_CS_PIN> psx;

// === Global vars end =======================================

void driver_init(driver_config_t *config)
{
  pinMode(config->pwm_pin, OUTPUT);
  pinMode(config->in1_pin, OUTPUT);
  pinMode(config->in2_pin, OUTPUT);
  pinMode(config->stby_pin, OUTPUT);
  analogWrite(config->pwm_pin, 0);
  digitalWrite(config->in1_pin, LOW);
  digitalWrite(config->in2_pin, LOW);
  digitalWrite(config->stby_pin, LOW);
}

void process_psx_buttons()
{
  //
}

void process_psx_analog()
{
  byte x, y;

  if (psx.getLeftAnalog(x, y)) {
    if ((psx_state.analog_left.x != x) || (psx_state.analog_left.y != y)) {
      Serial.print (F("Left "));
      Serial.print (F(" analog: x = "));
      Serial.print (x);
      Serial.print (F(", y = "));
      Serial.println (y);

      psx_state.analog_left.x = x;
      psx_state.analog_left.y = y;
      psx_state.analog_left.changed = true;
    } else {
      psx_state.analog_left.changed = true;
    }
  }

  if (psx.getRightAnalog(x, y)) {
    if ((psx_state.analog_right.x != x) || (psx_state.analog_right.y != y)) {
      Serial.print (F("Right "));
      Serial.print (F(" analog: x = "));
      Serial.print (x);
      Serial.print (F(", y = "));
      Serial.println (y);

      psx_state.analog_right.x = x;
      psx_state.analog_right.y = y;
      psx_state.analog_right.changed = true;
    } else {
      psx_state.analog_right.changed = true;
    }
  }
}

void driver_move(driver_config_t *config, int speed)
{
  if (speed > 0) {
    digitalWrite(config->in1_pin, HIGH);
    digitalWrite(config->in2_pin, LOW);
    analogWrite(config->pwm_pin, speed);
    digitalWrite(config->stby_pin, HIGH);
  } else if (speed < 0) {
    digitalWrite(config->in1_pin, LOW);
    digitalWrite(config->in2_pin, HIGH);
    analogWrite(config->pwm_pin, abs(speed));
    digitalWrite(config->stby_pin, HIGH);
  } else {
    digitalWrite(config->stby_pin, LOW);
  }
}

void adjust_driver()
{
  // Переводим в диапозон [-255; 255]
  int x = map(psx_state.analog_left.x, 0, 255, -255, 255);
  int y = map(psx_state.analog_left.y, 0, 255, -255, 255);

  // Зона нечувствительности (люфт стика в центре)
  if (abs(x) < 15) x = 0;
  if (abs(y) < 15) y = 0;

  // Алгоритм микширования для дифференциального шасси
  int leftSpeed = y + x;
  int rightSpeed = y - x;
  static int leftSpeedOld, rightSpeedOld = 0;

  // Ограничиваем скорость в пределах ШИМ (-255 ... 255)
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if ((leftSpeed != leftSpeedOld) || (rightSpeed != rightSpeedOld)) 
  {
    Serial.print (F("Left "));
    Serial.print (leftSpeed);
    Serial.print (F(" Right "));
    Serial.println (rightSpeed);
  }

  driver_move(&driver_config_left, leftSpeed);
  driver_move(&driver_config_right, rightSpeed);
}

void setup() 
{
  Serial.begin (115200);

  // Driver init
  driver_init(&driver_config_left);
  driver_init(&driver_config_right);

  Serial.println (F("Setup complete!"));
}

void loop() 
{
  if (!psx_state.detected) {
    if (psx.begin ()) {
      Serial.println (F("Controller found!"));
      delay (300);
      if (!psx.enterConfigMode ()) {
        Serial.println (F("Cannot enter config mode"));
      } else {
        if (!psx.enableAnalogSticks ()) {
          Serial.println (F("Cannot enable analog sticks"));
        }
        
        if (!psx.enableAnalogButtons ()) {
          Serial.println (F("Cannot enable analog buttons"));
        }
        
        if (!psx.exitConfigMode ()) {
          Serial.println (F("Cannot exit config mode"));
        }
      }

      psx_state.detected = true;
    }
  } else {
    if (!psx.read ()) {
      Serial.println (F("Controller lost :("));
      psx_state.detected = false;
    } else {
      if (psx.buttonsChanged()) {
        Serial.println (F("PSX buttonsChanged"));
        process_psx_buttons();
      }

      process_psx_analog();

      adjust_driver();
    }
  }

  // Only poll "once per frame" ;)
  delay(1000 / 60);
}
