# Subir el ZIP manualmente
from google.colab import files
import zipfile

uploaded = files.upload()

# Descomprimir el archivo ZIP
with zipfile.ZipFile("dataset.zip", 'r') as zip_ref:
    zip_ref.extractall("/content")

# Ruta correcta a tu dataset ya descomprimido
dataset_path = "/content/dataset"

import tensorflow as tf
from tensorflow.keras import layers, models
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.preprocessing import image_dataset_from_directory

# Validacion
train_ds = image_dataset_from_directory(
    dataset_path,
    validation_split=0.2, #separamos validacion y entrenamiento
    subset="training", #entrenamos
    seed=123,#division de imagenes siempre sea la misma
    image_size=(224, 224),
    batch_size=16 #grupos de 16 imagenes
)

val_ds = image_dataset_from_directory(
    dataset_path,
    validation_split=0.2,
    subset="validation",
    seed=123,
    image_size=(224, 224),
    batch_size=16
)


# Aleatoriedad
data_augmentation = tf.keras.Sequential([
    layers.RandomFlip("horizontal"), #giramos horizontalmente para mejor entrenamiento
    layers.RandomRotation(0.1),
    layers.RandomZoom(0.1),
])

#Escala de a 8 bits de colores
normalization_layer = layers.Rescaling(1./255)

# Aplicar augmentación y normalización solo al set de entrenamiento
train_ds = train_ds.map(lambda x, y: (data_augmentation(normalization_layer(x), training=True), y))
val_ds = val_ds.map(lambda x, y: (normalization_layer(x), y))

# Mejor rendimiento de entrenamiento para la GPU de googlecolab
AUTOTUNE = tf.data.AUTOTUNE
train_ds = train_ds.prefetch(buffer_size=AUTOTUNE)
val_ds = val_ds.prefetch(buffer_size=AUTOTUNE)


# MODELO
base_model = MobileNetV2(input_shape=(224, 224, 3), include_top=False, weights='imagenet')#top false para crear nuestras propias clases
base_model.trainable = False  # Congelamos las capas base

model = models.Sequential([
    base_model,
    layers.GlobalAveragePooling2D(),  #Reduce cada mapa de activación a un valor promedio (reduce parámetros).
    layers.Dense(3, activation='softmax')  #3 clases de señales
])
#mostrar en terminal el entrenamiento
model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',#perdida
              metrics=['accuracy'])#precision

# ENTRENAMIENTO

history = model.fit(
    train_ds,
    validation_data=val_ds,
    epochs=25#25 vueltas de entrenamiento
)

model.save("modelo_senales.h5")