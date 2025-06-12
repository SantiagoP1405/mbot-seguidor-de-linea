
import numpy as np
import cv2
import math
import serial
import time

from bluedot.btcomm import BluetoothServer
from time import sleep
from signal import pause

cap = cv2.VideoCapture(0)
ser = serial.Serial('/dev/ttyUSB0', 250000, timeout=1)
ser.reset_input_buffer()

bt_ros2 = ""
def data_received(data):
    global bt_ros2
    bt_ros2 = data
    print(data)
    server.send(data)

def client_connected():
    print("client connected")

def client_disconnected():
    print("client disconnected")

print("init")
server = BluetoothServer(
    data_received,
    auto_start = False,
    when_client_connects = client_connected,
    when_client_disconnects = client_disconnected)

print("starting")
server.start()
print(server.server_address)
print("waiting for connection")

while True:
    ret, img_bgr = cap.read()

    img_grey = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY) 
    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    img_blur = cv2.GaussianBlur(img_grey, (3, 3), 0, 0)
    img_canny = cv2.Canny(img_blur, 40, 120) 

    vertices = np.array([[(0,480), (150, 0), (450, 0), (640, 480)]], dtype=np.int32)    
    img_roi = np.zeros_like(img_grey)
    cv2.fillPoly(img_roi, vertices, 255)
    img_mask = cv2.bitwise_and(img_canny, img_roi)

    #cv2.imshow("roy",img_roi)

    rho = 2
    theta = np.pi / 180
    threshold = 40
    min_line_len = 50 #lineas cortas las ignora
    max_line_gap = 10    # permite lineas cercanas
    #Detecta líneas rectas a partir de los bordes usando transformada de hough
    lines = cv2.HoughLinesP(img_mask, rho, theta, threshold, np.array([]), minLineLength=min_line_len, maxLineGap=max_line_gap) 
    
    img_lines = np.zeros((img_mask.shape[0], img_mask.shape[1], 3), dtype=np.uint8) #se crea la imagen vacia para dibujar las lineas
    suma = 0
    contador = 0

    suma_centros_x = 0
    lineas_validas = 0

    #Si hay líneas detectadas, se recorre cada una.
    #Cada línea se representa por sus extremos (x1, y1) a (x2, y2)

    if lines is not None:
        for line in lines:
            for x1, y1, x2, y2 in line:
                centro_x = (x1 + x2) // 2 #Se calcula el centro horizontal (x) de la línea.
                suma_centros_x += centro_x #sto permite estimar si las líneas están más a la izquierda o derecha de la imagen. 
                #Se acumulan para sacar un promedio luego.
                lineas_validas += 1
                #pendientes de lineas para sacar angulos
                y = y2 - y1
                x = x2 - x1
                if x == 0:
                    continue

                r = y / x
                en_radianes = math.atan(r)#sacamos angulo en radianes
                en_grados = math.degrees(en_radianes)#convertimos a grados
                if en_grados < 0:
                    en_grados += 180 #Si el ángulo es negativo (caso de líneas inclinadas hacia la izquierda), se ajusta a 180

                cv2.line(img_lines, (x1, y1), (x2, y2), [255, 0, 0], 30)#se dibuja la linea azul para visualizar
                contador += 1
                suma += abs(en_grados)#sacamos suma de grados para sacar promedio

    promedio = suma / contador if contador > 0 else 90
    ang_conduccion = 0
    direccion = "F"

    umbral_giro = 5
    if abs(promedio - 90) < umbral_giro:
        ang_conduccion = 0
        direccion = "F"
    elif promedio < 90:
        ang_conduccion = min(90 - promedio, 45)
        direccion = "L"
    else:
        ang_conduccion = min(promedio - 90, 45)
        direccion = "R"

    height, width = img_mask.shape
    
    zona_central_min = width // 2 - 50
    zona_central_max = width // 2 + 50
    esquina_izq = 165 
    esquina_der = 460
    if lineas_validas > 0:
        centro_promedio_x = suma_centros_x / lineas_validas
        if centro_promedio_x < esquina_izq:
            direccion = "I"
            ang_conduccion = 20
        elif centro_promedio_x > esquina_der:
            direccion = "D"
            ang_conduccion = 20
        elif centro_promedio_x < zona_central_min:
            direccion = "L"
            ang_conduccion = 6
        elif centro_promedio_x > zona_central_max:
            direccion = "R"
            ang_conduccion = 6
    
    
    angulo_redondeado = f"{int(math.floor(ang_conduccion)):02d}"
    
    #Esto permite tener control manual temporal sobre el robot
    #Si desde Bluetooth se envía "F" (adelante), "L" (izquierda) el robot obedece ese comando una vez.
    # Si no se recibe nada, el robot sigue tomando decisiones con la cámara.
    if bt_ros2 !="":
        direccion_final=bt_ros2
        bt_ros2 = ""
        mensaje_mbot = str(angulo_redondeado) + direccion_final
    else:
        mensaje_mbot = str(angulo_redondeado) + direccion

    alpha = 1
    beta = 1
    gamma = 1
    #se obtiene una nueva imagen con las líneas superpuestas en color azul sobre la cámara en vivo, y se puede mostrar con cv2.imshow().
    img_lane_lines = cv2.addWeighted(img_rgb, alpha, img_lines, beta, gamma) 
    cv2.imshow('final',img_lane_lines)
    print("Mensaje Mbot:", mensaje_mbot)
    ser.write((mensaje_mbot + "\n").encode())
    ser.flush()
    #line = ser.readline().decode('utf-8').rstrip()
    line = ser.readline().decode('utf-8', errors='ignore').rstrip()
    print(line)

    time.sleep(0.1)
    if cv2.waitKey(1) == 13:
        break

cap.release()
cv2.destroyAllWindows()