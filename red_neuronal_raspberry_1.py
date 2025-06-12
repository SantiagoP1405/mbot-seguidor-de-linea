import numpy as np
import cv2
import time
import tflite_runtime.interpreter as tflite
from bluedot.btcomm import BluetoothClient
from time import sleep

def data_received(data):
    # En este caso el cliente es el que recibe, aquí podría quedar vacío o para debug
    print("recv - {}".format(data))

# Inicializa conexión Bluetooth (pon la MAC correcta de la Raspberry receptor)
print("Connecting Bluetooth Client...")
c = BluetoothClient("D8:3A:DD:0D:30:36", data_received)

# Cargar modelo TFLite
interpreter = tflite.Interpreter(model_path="modelo_senales.tflite")
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

cap2 = cv2.VideoCapture(0)

try:
    while True:
        ret2, img_nn = cap2.read()
        if not ret2:
            continue

        img_rgb_nn = cv2.cvtColor(img_nn, cv2.COLOR_BGR2RGB)
        img_resized = cv2.resize(img_rgb_nn, (224, 224))
        input_data = img_resized.astype(np.float32) / 255.0
        input_data = np.expand_dims(input_data, axis=0)

        cv2.imshow('Ventana', img_nn)

        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
        output = interpreter.get_tensor(output_details[0]['index'])[0]
        pred_clase = np.argmax(output)
        confianza = output[pred_clase]

        if confianza > 0.95:
            if pred_clase == 0:
                c.send('A')
            elif pred_clase == 2:
                c.send('S')
            elif pred_clase == 1:
                c.send('C')


        # Para que puedas cerrar la ventana con ESC
        if cv2.waitKey(1) & 0xFF == 27:
            break

finally:
    cap2.release()
    cv2.destroyAllWindows()
    c.disconnect()