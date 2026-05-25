# Mitsubishi Movemaster EX (RV-M1) - Robot Retrofitting & Control System

Este repositorio contiene el ecosistema completo de software, firmware y documentación técnica para el proyecto de modernización y *retrofitting* del brazo robótico industrial **Mitsubishi Movemaster EX (RV-M1)**. El objetivo principal del proyecto ha sido sustituir la electrónica propietaria obsoleta por una arquitectura abierta y escalable, capaz de realizar tareas de adquisición de datos en tiempo real (cada 4 ms) y control modular de alta velocidad sin latencias.

## 📌 Características Principales
* **Protocolo Industrial Inspirado en Modbus:** Comunicación binaria síncrona optimizada de tramas fijas (31 bytes) a un baudrate elevado de 500,000 bps.
* **Adquisición de Datos Determinista:** Lectura cíclica de encoders mediante interrupción de temporizador por hardware (Timer 5) a 10 kHz, decodificación *Half-Quad* y almacenamiento seguro a través de bloques atómicos.
* **HMI Modular en LabVIEW:** Estación de control basada en el patrón de diseño Productor-Consumidor (Queued Message Handler) con carga dinámica de subpaneles e interconexión segura de recursos vía Variables Globales Funcionales (FGV).
* **Compatibilidad Multi-Plataforma:** Firmware base desarrollado para Arduino Mega 2560 con infraestructura preparada en el código fuente para migración e implementación futura en ESP32.

## 📂 Estructura del Repositorio

El repositorio se organiza de la siguiente manera:

```text
├── documentacion_de_referencia/   # Tesis e investigaciones previas de estudiantes que renovaron el robot.
├── hardware/                      # Archivos de diseño electrónico y mapeo físico.
│   └── terminales_robot.xlsx      # Excel maestro con el pinout del conector SDP y código de colores.
├── manual_latex/                  # Código fuente en LaTeX del reporte de investigación y manual de usuario.
└── src/                           # Código fuente del sistema de control.
    ├── code/                      # Firmware de bajo nivel para los microcontroladores.
    │   ├── arduino_mega/          # Firmware de adquisición, control de frenos y comunicación Modbus.
    │   └── esp32/                 # Código base para la futura adaptación y migración de hardware.
    ├── labview/                   # Código de la HMI, lectura de sensores, FGVs y arquitectura de loops.
    └── rutinas/                   # Archivos de trayectorias (Excel/CSV) cargables para rutinas de movimiento.

### 3. Inicialización Global (`Global Init`)
```markdown
## 🛠️ Detalles de la Arquitectura de Software y Hardware

### 1. Inicialización Global (`Global Init`)
Ubicado en `src/labview/`, centraliza las constantes críticas del protocolo y la configuración física de la sesión serial VISA para un comportamiento determinista:
* `T_muestreo` (0.004 s): Intervalo de muestreo (4 ms) del Arduino para el cálculo exacto de velocidad.
* `Pack_t` (31 bytes): Tamaño fijo de la trama de telemetría individual para evitar desfases en la des-serialización.
* `Tramas` (15): Cantidad de paquetes acumulados por lote en cada iteración de lectura de LabVIEW.
* `Bytes_at_port`: Constante calculada dinámicamente (`Pack_t` × `Tramas`) que define el tamaño exacto del lote de datos esperado en el búfer antes de su desempaquetado.
* `V_buf_t` (10,000 bytes): Búfer asignado a la sesión VISA para evitar desbordamientos ante ráfagas de datos.
* `V_timeout` (100 ms) y `V_baud` (500,000 bps): Configuración de alta velocidad para la capa física de comunicación serie.

### 2. Flujo Lógico de Adquisición de Telemetría (Lazo VISA)
El bucle de adquisición lee el búfer serial continuamente y opera bajo una máquina de estados de 5 pasos:

```mermaid
graph TD
    Start[Start: Inicializa VISA y resetea valores] --> WriteData{Write Data: ¿Sesión VISA iniciada?}
    WriteData -- No --> Wait[Espera activa / Idle]
    WriteData -- Sí --> Decode[Llama a Decode Modbus Message]
    Decode --> Distribute[Manda datos en Arrays a FGV CSV y Dashboard]
    Distribute --> WriteData
    Reconnect[Reconnect: Cierra VISA previa, reinicia con nuevo indicador] --> Idle[Retorna a reposo / Idle]
    Stop[Stop: Cierra VISA, vacía buffers y reinicia Shift Registers e Indicadores] --> End([Fin])

### 5. Decodificación de Datos (`Decode Modbus Message`)
```markdown
### 3. Decodificación de Datos (`Decode Modbus Message`)
Este VI des-serializa secuencialmente los lotes de telemetría mediante una función *Unflatten from String* con configuración *Little Endian*:
1.  **Definiciones Clave:**
    * **Lote de Datos (*Paquetote*):** Bloque masivo de bytes recibidos en el búfer serial cuyo tamaño mínimo es igual a $\text{Longitud de Trama (31)} \times \text{Número de Paquetes}$.
    * **Trama:** Mensaje individual atómico de 31 bytes que contiene de forma compacta el estado operativo y los pasos de los 6 motores, sensores de límite y banderas de error.
2.  **Proceso:** Un ciclo `While` extrae trama por trama del lote de datos, realiza el *Unflatten*, concatena los valores descifrados en arrays a la terminal de salida y los distribuye sincrónicamente hacia los lazos del subpanel del Dashboard y almacenamiento CSV.

### 4. Concurrencia y Estructuración de Mensajes (`FGV VISA Write`)
Para prevenir condiciones de carrera y colisiones de datos por accesos paralelos concurrentes al puerto serie, se implementó una Variable Global Funcional (`FGV VISA Write`) con registros de desplazamiento internos (*Shift Registers*). Asimismo, estandariza el envío de paquetes binarios empaquetados hacia el Arduino:

| Estado / Acción | Estructura de la Trama de Control | Función Técnica |
| :--- | :--- | :--- |
| **SET / GET REF** | Referencia de sesión de VISA interna | Almacena y recupera de forma segura la sesión VISA en el *Shift Register*. |
| **SEND ROUTINE** | `Header (0x3A)` \| `FX = 3` \| `Payload = 0` \| `PWM (Int16)` \| `Tiempo (Int16)` \| `CRC` | Envía secuencialmente las filas de trayectoria leídas desde el archivo de Excel. |
| **EMERGENCY STOP**| `Header (0x3A)` \| `FX = 1` \| `Payload = 0` \| `Array de ceros` \| `CRC` | Envía una trama de parada de prioridad crítica que apaga inmediatamente todos los PWM. |
| **MOVE MANUAL** | `Header (0x3A)` \| `FX = 1` \| `PWM` \| `Dirección/Pasos (±400)` \| `CRC` | Vinculado a los controles manuales discretos; avanza exactamente 400 pasos por pulso. |

## 🖥️ Interfaz de Usuario e Integración de Subpanels

### Estado Inicial de Reposo (Sin Inicializar)
Cuando el software principal de LabVIEW no ha sido inicializado, la HMI se presenta en un estado de reposo seguro:
* **Región Central (*Hueco*):** Se observa un espacio vacío dedicado al contenedor de subpaneles dinámicos.
* **Controles Globales Visibles:** En los laterales se encuentran dispuestos de forma fija los 3 botones selectores de vista de subpanels, controles maestros de ejecución (`Start`/`Stop`), botón de Paro de Emergencia, módulo de Guardar datos en CSV, indicadores de seguridad de los Finales de Carrera y la carátula del monitor de errores del sistema.

### Subpanel: Dashboard (`Dashboard.vi`)
Diseñado para la visualización fluida de instrumentos analógicos de telemetría sin congelamientos de la interfaz gráfica de usuario. Utiliza una arquitectura orientada a eventos:
* **Manejador de Eventos:** Recibe el array de telemetría proveniente del lazo de adquisición y actualiza los indicadores numéricos y carátulas dinámicas mediante el VI `Refresh Ind`.
* **Cierre Seguro:** Cuando el evento del subpanel se destruye, detiene automáticamente su ciclo interno.

### Subpanel: Control Manual (`Control.vi`)
Proporciona control granular sobre los actuadores del brazo robótico mediante tres modalidades de comando:
1.  **Sliders de Ejes:** Envían comandos continuos de PWM a la `FGV VISA Write` haciendo que la articulación avance de manera sostenida en la dirección deseada hasta alcanzar de manera segura un final de carrera físico.
2.  **Pulsadores Manuales (`B_izq` / `B_der`):** Desplazan la articulación seleccionada exactamente 400 pasos discretos en el sentido indicado, utilizando el ciclo de trabajo fijado en el indicador PWM. El botón de **Pausa** envía un vector de ceros al Arduino forzando la detención inmediata del movimiento.
3.  **Reproducción de Rutinas (`Play File`):** Toma el archivo de Excel cargado y lo recorre de forma jerárquica (primero por filas de tiempo/PWM y luego mapeando por columnas de articulación). Envía la información a través de la FGV de escritura y **espera de manera determinista la confirmación de llegada por parte del Arduino** antes de transmitir la siguiente línea del archivo.

### Lazo de Registro de Datos (`Data Logger CSV`)
Gobernado por la Variable Global Funcional `FGV CSV`, este lazo paralelo e independiente de la interfaz gráfica asegura el almacenamiento masivo de datos en disco:

| Estado / Paso | Operación de Software |
| :--- | :--- |
| **Start** | Se gatilla al presionar el botón de inicio de guardado dentro de la sección dedicada de la interfaz. Crea un archivo en formato de Excel con el prefijo `test_` en el directorio indicado por el usuario. |
| **Log_Data_All**| Transita automáticamente desde *Start*. Lee cíclicamente los arreglos de la telemetría activa y añade filas al archivo CSV registrando de forma masiva los pasos, vueltas y la velocidad calculada de todos los encoders. |
| **Stop** | Detiene de manera obligatoria la escritura cíclica al presionar el botón de Stop en la interfaz, cerrando el flujo del archivo de forma segura para evitar pérdida de datos o corrupción. |

## 🚀 Requisitos e Instalación

1.  **LabVIEW:** Versión 2020 o posterior con los controladores **NI-VISA** instalados.
2.  **Arduino IDE:** Para compilar y cargar el firmware en la carpeta `src/code/arduino_mega/` (requiere soporte nativo para operaciones atómicas de AVR).
3.  **Excel/Herramienta CSV:** Necesario para el diseño e interpretación de las hojas de trayectorias en `src/rutinas/`.
