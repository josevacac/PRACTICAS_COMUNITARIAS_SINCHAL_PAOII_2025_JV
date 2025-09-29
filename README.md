Arquitectura de la Red

Transmisor (Reservorio):

Mide el nivel de agua con un sensor analógico calibrado por voltajes.

Aplica filtrado de lecturas mediante el método IQR.

Convierte el voltaje en nivel (cm) con interpolación lineal.

Envía solo el nivel promedio al Receptor vía LoRa.

Receptor (Junta de Agua):

Recibe los datos LoRa del transmisor.

Muestra información en pantalla OLED.

Publica los datos en ThingSpeak mediante MQTT.

ThingSpeak:

Field 1 → Nivel (cm).

Field 2 → Caudal (L/min, usado en pruebas).




Componentes Utilizados

Módulos LoRa Heltec WiFi LoRa 32 (V3).

Sensor de nivel sumergible (analógico).

Caudalímetro (usado en pruebas).

Panel solar + batería + controlador de carga.

Plataforma ThingSpeak para visualización de datos.




Códigos Incluidos en este Repositorio

Transmisor_Reservorio_Final.ino → código definitivo, calibrado por voltajes, cargado en el transmisor del reservorio.

Receptor_LoRa_Final.ino → código del receptor instalado en la Junta de Agua (publica en ThingSpeak).

NodoCaudal_Simulador.ino → simulador del nodo caudalímetro (para pruebas).

Transmisor_Reservorio_Relay.ino → transmisor de pruebas que combina caudal + nivel.





Configuración en ThingSpeak

Se creó un nuevo canal para el proyecto.

Field 1: Nivel de agua en cm.

Field 2: Caudal (solo para pruebas).

Conexión mediante MQTT con credenciales propias del canal.





Resultados Principales

Comunicación LoRa estable entre reservorio y Junta (>2 km de distancia).

Nivel de agua en cm publicado en tiempo real en ThingSpeak.

Validación de pruebas con nodo caudalímetro, confirmando que la red es escalable.

Coherencia entre mediciones del sensor y mediciones manuales realizadas en campo.






Evidencias

Fotografías de:

Montaje en el reservorio.

Instalación en la Junta de Agua.

Pozo con sensor sumergible.

Capturas de ThingSpeak con los gráficos de nivel y caudal.
