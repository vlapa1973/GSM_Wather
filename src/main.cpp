#include <Arduino.h>
#include <EEPROM.h>         // Стандартная библиотека для записи в энергонезависимую память
#include <SoftwareSerial.h> // Стандартная библиотека для виртуального последовательного порта
#include <LowPower.h>       // Библиотека для эффективного засыпания - https://github.com/rocketscream/Low-Power

SoftwareSerial SIM800(18, 19); // Виртуальные RX, TX

#define timer 160200 // 7*24*6675 tick/hour - через сколько тиков по ~0,5с отправлять смс

boolean hotPrev; // Переменные
boolean coldPrev;
unsigned long ticks = 0;
float hot, cold;

void setup()
{
  //  delay(10000); // Необходама однократная прошивка начальных значений счетчиков перед установкой
  //  EEPROM.put(0, 32.385);
  //  EEPROM.put(20, 47.8910);

  Serial.begin(115200);

  EEPROM.get(0, hot);
  EEPROM.get(20, cold);

  pinMode(2, INPUT);  // Вход с геркона счетчика ГВС
  pinMode(3, INPUT);  // Вход с геркона счетчика ХВС
  pinMode(4, OUTPUT); // Управление транзистором, через который подключен GSM модуль

  digitalWrite(2, HIGH); // Определяем начальное состояние счетчика ГВС
  hotPrev = digitalRead(2);
  digitalWrite(2, LOW);

  digitalWrite(3, HIGH); // Определяем начальное состояние счетчика ХВС
  coldPrev = digitalRead(3);
  digitalWrite(3, LOW);

  for (int i = 0; i < 3000; i++)
  {
    Serial.println("test");
    checkCounter();
  }
  Serial.println("test SMS");
  sendSMS(); // Отправка тестового СМС
}

void loop()
{
  checkCounter(); // Запускаем функцию опроса герконов счетчика
  ticks++;        // Тикаем один раз
  if (ticks > timer)
  { // Если кол-во тиков превысило заданное, обнуляем счетчик, записываем данные в EEPROM и шлем смс
    ticks = 0;
    sendSMS();
  }
}

void checkCounter()
{ //Функция опроса счетчиков

  digitalWrite(2, HIGH); //Подтягивем пин 2 к Vcc через встроенный в МК резистор 20кОм
  if (digitalRead(2) != hotPrev)
  {                     // Проверяем не изменилось ли состояние пина, если да, то:
    hot = hot + 0.0005; // -накидываем поллитруху
    hotPrev = !hotPrev; // -инвертируем переменную
  }
  digitalWrite(2, LOW); // Отключаем подтяжку к Vcc (если этого не зделать, то МК будет жрать повышенный ток 4,2В/20кОм = 210мкА! с пина 2 и столько же с пина 3)

  digitalWrite(3, HIGH); // Все аналогично счетчику ГВС
  if (digitalRead(3) != coldPrev)
  {
    cold = cold + 0.0005;
    coldPrev = !coldPrev;
  }
  digitalWrite(3, LOW);

  //  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF); // Засыпаем на 0,5с
}

void sendSMS()
{ // Функция отправки смс

  EEPROM.put(0, hot);
  EEPROM.put(20, cold);

  float V = 0; // Переменная для напряжения питания
  for (int i = 0; i < 100; i++)
  {                                     // Сто раз считываем значение с пина A3 (который подключен к Vcc) и внутреннее опорное напряжение
    V = V + analogRead(A3) / readVcc(); // Суммируем
  }
  V = (V / 100) * 1.0656 + 0.1017; // Усредняем с учетом коэффициентов, откалиброванных при Vcc 3,0-4,2В

  digitalWrite(4, HIGH); // Включаем GSM модуль
  Serial.println("send SMSsssss");
  for (int i = 0; i < 100; i++)
    checkCounter();               //Даем модулю 50 сек на установку связи, в это время опрашиваем счетчики
  SIM800.begin(9600);             // Скорость обмена данными с модемом
  SIM800.flush(), checkCounter(); // Дожидаемся передачи команды по Serial и даем GSM модулю 0,5 с на выполнение команды (проверяем в это время счетчики)
  SIM800.println("AT");           // Автонастройка скорости
  SIM800.flush(), checkCounter();
  SIM800.println("AT+CMGF=1"); // Включить TextMode для SMS
  SIM800.flush(), checkCounter();
  SIM800.println("AT+CMGS=\"+79260009333\""); // Номер, на который шлем смс
  SIM800.flush(), checkCounter();
  SIM800.println("C: " + String(cold, 3) + ", H: " + String(hot, 3) + ", V: " + String(V, 3) + "\r\n" + (String)((char)26)); // Строка смс сообщения: ХВС, ГВС, напряжение на батарее
  SIM800.flush();
  for (int i = 0; i < 60; i++)
    checkCounter();             // Даем модулю 30 секунд на отправку смс, в это время опрашиваем счетчики
  SIM800.println("AT+CPOWD=1"); // Команда выключения
  SIM800.flush();
  for (int i = 0; i < 10; i++)
    checkCounter();     // 5 сек даем на отключение
  digitalWrite(4, LOW); // Обесточиваем GSM модуль
}

float readVcc()
{ // Функция, возвращающая внутреннее опорное напряжение МК
  float tmp = 0.0;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC))
    ;
  uint8_t low = ADCL;
  uint8_t high = ADCH;
  tmp = (high << 8) | low;
  return tmp;
}