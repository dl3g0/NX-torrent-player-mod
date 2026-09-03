# 🚀 NX Torrent Player MOD v0.0.4 — Release Notes

¡Bienvenidos a la versión **v0.0.4** de **NX Torrent Player (MOD)** creada por **dl3g0**!

Esta actualización añade la función de **reintento de streams con el botón `Y`**, implementa un **límite de tiempo de espera inteligente (15s)** para erradicar las cargas infinitas, optimiza la carga de fondos y logos directamente desde el almacenamiento local y **rediseña la pantalla de carga/error** eliminando elementos congelados y cajas invisibles.

---

## 🌟 Novedades Principales en v0.0.4

### 🔄 1. Reintento Rápido de Streams con Botón `Y`
* **Reconexión Instantánea sin Salir al Menú**:
  * Si un stream HTTP / Debrid o Torrent se queda colgado, experimenta lentitud o arroja un error de red/enlace caído, ahora puedes presionar **`Y` (Reintentar)** para reiniciar la petición de inmediato.
  * Durante la reproducción normal, el botón **`Y`** conserva su función de bloqueo/desbloqueo de controles (*Lock Controls*).

---

### ⏱️ 2. Detección de Cargas Lentas y Límite de Espera (Timeout a los 15s)
* **Fin a los Cuelgues y Cargas Infinitas**:
  * Si un servidor tarda más de **15 segundos** en responder, la app cancela automáticamente la espera indefinida y muestra el mensaje de fallo junto con la opción de reintentar con **`Y`** o volver con **`B`**.
  * Si la carga sobrepasa los **6 segundos**, la interfaz notifica de forma dinámica: *"Demorando más de lo habitual... Presiona Y para reintentar"*.

---

### 🎨 3. Carga Inmediata de Fondos 16:9 y Logotipos Oficiales (Cero Parpadeos)
* **Renderizado Directo desde Almacenamiento Local**:
  * Los fondos panorámicos 16:9 (`.bg.jpg`) y los logos oficiales transparentes (`.logo.png`) se leen desde la caché en disco desde el primer fotograma (0 ms).
  * Se eliminó por completo el parpadeo donde antes aparecía momentáneamente la carátula vertical estirada antes de cargar el fondo definitivo.

---

### 📐 4. Rediseño Limpio de la Pantalla de Carga y Error
* **Eliminación de Elementos Fantasma y Cajas Descuadradas**:
  * En caso de error o tiempo de espera agotado, la barra de progreso, los porcentajes y el círculo de carga se ocultan por completo, evitando espacios vacíos o descuadres en el centro de la pantalla.
  * Los mensajes de error ahora se muestran centrados, con ajuste de línea automático (*word wrapping*) y en un tono suave.
  * Se eliminó la duplicidad del texto en pantalla para mantener una leyenda limpia en el pie de página: `"Presiona Y para reintentar  •  Presiona B para volver"`.

---

### 👆 5. Salto Rápido con Doble Toque Táctil y Botones `ZL` / `ZR` (*Multi-Step Seek*)
* **Avance y Retroceso Estilo Netflix / YouTube**:
  * **Pantalla Táctil**:
    * **Lado Derecho**: Doble toque avanza **+10 segundos** (`+10s ⏩`). Toques continuos acumulan **+20s**, **+30s**, **+40s**...
    * **Lado Izquierdo**: Doble toque retrocede **-10 segundos** (`⏪ 10s`). Toques continuos acumulan **⏪ 20s**, **⏪ 30s**...
    * **Centro**: Un toque abre y cierra la barra de controles inmediatamente.
  * **Botones Físicos `ZL` / `ZR`**:
    * **Gatillo `ZR`**: Presionar 2 veces avanza **+10 segundos** (`+10s ⏩`). Pulsaciones continuas acumulan **+20s**, **+30s**, **+40s**...
    * **Gatillo `ZL`**: Presionar 2 veces retrocede **-10 segundos** (`⏪ 10s`). Pulsaciones continuas acumulan **⏪ 20s**, **⏪ 30s**...
* **Panel de Diagnóstico Trasladado al Menú de Opciones (`X`)**:
  * El overlay de estadísticas y debug ya no se activa accidentalmente con `ZR`. Ahora se habilita/deshabilita de forma limpia como una opción dentro del menú de configuración de reproducción (**`X` > DIAGNÓSTICO > Estadísticas detalladas**).

---

### 📏 6. Corrección de Superposición en Títulos Largos de Películas
* **Ajuste y Desplazamiento Automático (*Marquee*)**:
  * Se acotó el ancho máximo de la píldora superior izquierda de título a 560px con auto-scroll suave.
  * Ya no se sobrepone nunca más a los botones de la esquina superior derecha (*Opciones, Bloqueo, Velocidad*) ni al indicador de *Buffering*.

### 🛡️ 7. Optimización de Streaming Torrent 1080p y Corrección de Crash al Salir y Reentrar
* **Ampliación de Ventana de Streaming (64 MB) y Pre-descarga de Índices (MKV/MP4)**:
  * Se amplió la ventana de streaming (`STREAM_WINDOW`) de 32 MB a 64 MB para alimentar con holgura los decodificadores de hardware con contenido 1080p de alto bitrate.
  * Se cuadruplicó la región crítica de cola (`CRIT_TAIL_BYTES` a 16 MB) para que los índices *Cues* de MKV y atom *moov* de MP4 se descarguen al instante sin pausar la reproducción.
* **Arranque Inmediato sin Bloqueos de Búfer**:
  * Se vinculó el evento `MPV_EVENT_PLAYBACK_RESTART` para despausar el vídeo en el instante exacto en que el primer fotograma es decodificado por hardware.
* **Eliminación de Crashes por Colisión de Motores y Sockets BSD**:
  * Se implementó exclusión mutua global (`s_torrentEngineMutex`) en el ciclo de vida del motor `torrentfs`. Si sales de un stream con `B` y vuelves a entrar de inmediato, la nueva instancia espera de forma segura a que la anterior libere sus hilos y sockets BSD, evitando corrupción de caché cruzada y cierres forzados del sistema operativo.

---

## 📥 Instalación

1. Descarga el archivo `NX-torrent-player.nro` adjunto en este release.
2. Cópialo a tu tarjeta microSD en:
   ```text
   sdmc:/switch/NX-torrent-player/NX-torrent-player.nro
   ```
3. Inicia la aplicación desde el **Homebrew Menu** en tu Nintendo Switch (se recomienda ejecutarlo en modo Title Override / manteniendo `R` sobre cualquier juego para disponer de toda la memoria RAM).

---

## 👏 Créditos y Agradecimientos

* **dl3g0** — Desarrollo y optimización de este mod/fork.
* **shodowlo** — Proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* [borealis](https://github.com/xfangfang/borealis), [mpv](https://mpv.io/), [libutp](https://github.com/bittorrent/libutp), [Stremio](https://www.stremio.com/), [OpenMoji](https://openmoji.org/) y devkitPro.
