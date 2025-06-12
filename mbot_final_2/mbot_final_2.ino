// Importación de las librerías
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include "src/MeBarrierSensor.h"
#include <MeMegaPi.h>

#define F_CPU 16000000UL
#define USART_BAUDRATE 250000 // Definimos puerto serial a 250000 baudios
#define UBRR_VAL ((F_CPU / (USART_BAUDRATE * 8UL)) - 1)

// Sensores y motores
MeBarrierSensor barrier_61(61);
MeBarrierSensor barrier_62(62);
MeBarrierSensor barrier_60(60);
MeMegaPiDCMotor motor_1(1);
MeMegaPiDCMotor motor_9(9);
MeMegaPiDCMotor motor_2(2);
MeMegaPiDCMotor motor_10(10);
MeMegaPiDCMotor motor_3(3);
MeMegaPiDCMotor motor_11(11);
MeMegaPiDCMotor motor_4(4);
MeMegaPiDCMotor motor_12(12);

// Configuración de velocidades globales
const int speed  = 59;
const int speed2 = 61;
const int spin   = 40;

volatile bool sensorPause = true;  /*Booleano que funciona como bandera para reanudar
o pausar el movimiento(true cuando A8 detecta algo, false cuando no)*/

//Booleanos que funciona como bandera para los estados proveídos por la red neuronal
bool stopSignal = false;
bool continueSignal = false;
bool turnSignal = false;


TaskHandle_t vTaskMainHandler; //Handler para la tarea principal. 

// Función para incializar la comunicación UART con doble velocidad.  
void initUART() {
  UCSR0A = (1 << U2X0);  // Modo doble velocidad ACTIVADO
  UBRR0H = (uint8_t)(UBRR_VAL >> 8);
  UBRR0L = (uint8_t)UBRR_VAL;
  UCSR0B = (1 << RXEN0) | (1 << TXEN0);
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 
}

// Función para transmitir un carácter de forma optimizada
void USART_Transmit(unsigned char data) {
  loop_until_bit_is_set(UCSR0A, UDRE0); //Espera a que el buffer esté listo para recibir
  UDR0 = data; //Envía el caracter al registro de datos USART
}

/* Función para transmitir una cadena de caracteres (envía los caracteres uno por uno)
hasta llegar a un salto de linea*/
void USART_Transmit_String(const char* str) {
  while (*str) {
    USART_Transmit(*str++);
  }
}

int getCharFromUART() {
  if (UCSR0A & (1 << RXC0)) { // Si recibe un byte
    return UDR0; //Se devuelve el valor recibido
  }
  return -1; // Devuelve -1 para ind icar que el valor no fue recibido
}
    

// Función de control de motores según el grado de giro y dirección
void controlMotors(int grado_giro, char direccion) {
  if(direccion == 'C'){ // Cuando la red neuronal envía que se detectó una curva
    turnSignal = true;  // Activar señal de giro
    continueSignal = false; // Desactivar continuar para bajar velocidad
    //stopSignal = false; // Desactivar señal de parada
  }else if (direccion == 'S'){//Cuando la red neuronal evía que se detectó la señal de avanzar
    //turnSignal = false; // Desactivar señal de giro
    continueSignal = true; // Activar continuar para subir velocidad
    stopSignal = false; // Desactivar señal de parada
  }else if (direccion == 'A'){//Cuando la red neuronal detecta una señal de alto.
    turnSignal = false; // Desactivar señal de giro
    continueSignal = false; // Desactivar continuar
    stopSignal = true; // Activar señal de parada
  }
  if (stopSignal == true){// Si la señal de alto está activa, entonces el robot no se mueve
    motor_2.run(0);          // izquierdo atrás
    motor_10.run(0);         // izquierdo adelante
    motor_1.run(0);          // derecho adelante
    motor_9.run(0);         // derecho atrás
  }
  else if (grado_giro >= 20) {//Zona de curva muy cerrada
    if (direccion == 'D' && turnSignal) { //Cuando se debe girar a la derecha y la señal de curva está activa
      motor_2.run(-speed - 26);   // izquierdo atrás
      motor_10.run(-speed - 26);  // izquierdo adelante
      motor_1.run(spin);          // derecho adelante
      motor_9.run(spin);          // derecho atrás
    }
    else if (direccion == 'I' &&  turnSignal) { //Cuando se debe girar a la izquierda y la señal de curva está activa
      motor_2.run(-spin);         // izquierdo atrás
      motor_10.run(-spin);        // izquierdo adelante
      motor_1.run(speed2 + 25);   // derecho adelante
      motor_9.run(speed2 + 25);   // derecho atrás
    }
    else if (direccion == 'R') {
      motor_2.run(-speed - 10);   // izquierdo atrás
      motor_10.run(-speed - 10);  // izquierdo adelante
      motor_1.run(spin);          // derecho adelante
      motor_9.run(spin);          // derecho atrás
    }
    else if (direccion == 'L') {
      motor_2.run(-spin);         // izquierdo atrás
      motor_10.run(-spin);        // izquierdo adelante
      motor_1.run(speed2 + 10);   // derecho adelante
      motor_9.run(speed2 + 10);   // derecho atrás
    }
  }
  else if (grado_giro >= 12) {//Zona de curva medianamente cerrada
    if (direccion == 'R') {
      motor_2.run(-speed - 8);    // izquierdo atrás
      motor_10.run(-speed - 8);   // izquierdo adelante
      motor_1.run(spin);          // derecho adelante
      motor_9.run(spin);
    }
    else if (direccion == 'L') {
      motor_2.run(-spin);         // izquierdo atrás
      motor_10.run(-spin);        // izquierdo adelante
      motor_1.run(speed2 + 8);    // derecho adelante
      motor_9.run(speed2 + 8);
    }
  }
  else if (grado_giro >= 6) {//Zona de curva muy poco cerrada
    if (direccion == 'R') {
      motor_2.run(-speed - 6);    // izquierdo atrás
      motor_10.run(-speed - 6);   // izquierdo adelante
      motor_1.run(spin);          // derecho adelante
      motor_9.run(spin);
    }
    else if (direccion == 'L') {
      motor_2.run(-spin);         // izquierdo atrás
      motor_10.run(-spin);        // izquierdo adelante
      motor_1.run(speed2 + 6);    // derecho adelante
      motor_9.run(speed2 + 6);
    }
  }
  
  else{//En caso de que la señal de alto no esté activa, el robot avanza hacia adelante
    motor_2.run(-speed);          // izquierdo atrás
    motor_10.run(-speed);         // izquierdo adelante
    motor_1.run(speed2);          // derecho adelante
    motor_9.run(speed2);
  }
  
}


//Habilitación de la interrupción en PCINT16
ISR(PCINT2_vect) {
  // PK0 corresponde a A8 en Arduino Mega (PCINT16)
  if (PINK & (1 << PINK0)) {
    // Si el sensor en A8 está detectando, entonces se detienen todos los motores
    sensorPause = true;
    motor_1.run(0);
    motor_2.run(0);
    motor_3.run(0);
    motor_4.run(0);
    motor_9.run(0);
    motor_10.run(0);
    motor_11.run(0);
    motor_12.run(0);
  }
  else {
    /* Si el sensor no detecta nada, se desactiva la bandera de la interrupción y se
    regresa a la tarea principal*/
    sensorPause = false;
  }
}

/*Tarea principal del programa. En esta se lleva a cabo la comunicación por UART con la raspberry 
y se manda a llamar la función para mover motores, según los datos entregados por la raspberry*/
void vTaskMain(void *pvParameters) {
  String data = ""; //Se inicializa la variable data para almacenar los caracteres que van llegando a través de UART
  for (;;) { //Ciclo infinito
    // Procesar todo lo que haya en el UART sin esperar ticks completos
    int r;
    while ((r = getCharFromUART()) != -1) { /*Se manda a llamar a la función para leer un caracter recibido por UART
        si no recibió nada, no ejecuta lo que está dentro del ciclo  while*/
      char c = (char)r; //Convierte el dato recibido a un caracter
      if (c == '\n') { //Esta condición no se cunple si no se ha detectado un salto de línea en el mensaje
        // Envia confirmación del mensaje completo
        USART_Transmit_String("You sent me: "); //Se regresa el dato recibido por puerto serial hacia la raspberry
        USART_Transmit_String(data.c_str());
        USART_Transmit('\n');

        //Se separa el ángulo de conducción de la dirección al 
        if (data.length() >= 2) {
          char direccion = data.charAt(data.length() - 1);
          String gradoStr = data.substring(0, data.length() - 1);
          int grado = gradoStr.toInt(); //Se castea el string del angulo a un entero. 
          
          // validación para cualquiera de las direcciones
          if (grado >= 0 && (direccion == 'D' || direccion == 'I' || 
              direccion == 'R' || direccion == 'L' || direccion == 'F' || direccion == 'A' || direccion == 'S' || direccion == 'C')) {
            // Se comprueba si el sensor no está pausando la ejecución
            if (sensorPause) { 
              controlMotors(grado, direccion); //Se manda a llamar la función para mover los motores
              USART_Transmit('!'); // Confirmación  de que se están moviendo los motores
            }
            // Si sensorPause == true, no hacemos nada con los motores (quedan detenidos)
          }
        }
        // Limpia la variable para el siguiente mensaje
        data = "";
      }
      else {
        data += c; // Mientras no haya salto de linea, los datos se concatenan a data 
      }
    }
    // Ceder la CPU para que otra tareas puedan ejecutarse. 
    taskYIELD();
  }
}

void vTaskSensor(void *pvParameters) { // Tara para llevar control sobre la interrupción
  static bool messageSent = false; //Bandera para verificar si se mandó el mensaje
  while(1){
    if(sensorPause == true){ // Si la bandera de la interrupción está activada, entonces se manda un mensaje
      if (!messageSent) {
        USART_Transmit_String("-------------------------------\n");
        USART_Transmit_String("----- OBSTÁCULO DETECTADO -----\n");
        USART_Transmit_String("-------------------------------\n");
        messageSent = true; //Se pone la bandera de que ya se mandó el mensaje para que se envíe solamente una vez
      }
    } else {
      messageSent = false;  // Se resetea la bandera para la próxima detección
    }
    //vTaskDelay(pdMS_TO_TICKS(100)); // Pausa para evitar la saturación del puerto serial. 
    taskYIELD();
  }
}

void setup() {
  initUART(); // Manda a inicializar la comunicación UART
  sei();  // Habilita interrupciones globales
  PCICR  |= (1 << PCIE2);       // Habilita PCINT
  PCMSK2 |= (1 << PCINT16);     // Activa PCINT16 (A8)
  
  // Inicializar motores en estado detenido
  motor_1.run(0);
  motor_2.run(0);
  motor_3.run(0);
  motor_4.run(0);
  motor_9.run(0);
  motor_10.run(0);
  motor_11.run(0);
  motor_12.run(0);

  // Crear la tarea principal
  xTaskCreate(vTaskMain, "Main", 256, NULL, 1, &vTaskMainHandler);
  // Crea la tarea de control del sensor de la interrupción
  xTaskCreate(vTaskSensor, "Estado Sensor", 256, NULL, 1, NULL);
  vTaskStartScheduler(); //Para llevaar el control de la ejecución de las tareas
}

void loop() {}
