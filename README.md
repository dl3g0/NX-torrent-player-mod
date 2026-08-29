<img src="https://github.com/shodowlo/NX-torrent-player/blob/main/assets/NX-torrent-player-banner.png?raw=true" alt="NX Torrent Player MOD" width="640">

# NX Torrent Player (MOD)

**NX Torrent Player (MOD): Stremio y Reproductor de Torrents, Streams & Descargas Offline para Nintendo Switch**

Versión: **v0.0.2 (MOD by dl3g0)**  
Repositorio: [https://github.com/dl3g0/NX-torrent-player-mod](https://github.com/dl3g0/NX-torrent-player-mod)  
Basado en el proyecto original **NX Torrent Player** creado por **shodowlo**.

> Requiere una consola Nintendo Switch con Custom Firmware (Atmosphère). Se recomienda ejecutar en modo *Title Override* (mantener presionado `R` sobre cualquier juego al abrirlo) para disponer de toda la memoria RAM de la consola.

---

## 🌟 Novedades y Características de este Fork (por dl3g0)

### 🆕 Lo Nuevo en la Versión v0.0.2

#### 🎬 1. Nuevo Reproductor Exclusivo para Archivos Locales y Descargas
- **Arquitectura Desacoplada (`LocalPlayerActivity` / `LocalMpvView`)**: Motor de reproducción local optimizado exclusivamente para archivos en la MicroSD y descargas finalizadas, libre de sobrecargas de red o P2P.
- **Interfaz y Controles Táctiles Completos**:
  - Panel lateral de configuración translúcido (`X`) con selector de pistas de audio, subtítulos incrustados/externos, tamaño y retardo de subtítulos (`sub-delay`) y velocidad de reproducción (0.5x - 2.0x).
  - Gestos táctiles en barra de búsqueda con previsualización en tiempo real.
  - Bloqueo de controles con botón **`Y`** para evitar toques accidentales.
  - Adelantar y retroceder continuo con el **Joystick analógico** con aceleración progresiva y sin pausar la reproducción.

#### 📴 2. Gestor de Descargas Offline a la MicroSD (Debrid + Torrents P2P)
- **Descarga de Contenido Completo en Segundo Plano**: Descarga películas o capítulos enteros a `sdmc:/switch/NX-torrent-player/downloads/` para verlos sin conexión a internet (ideal para viajes o modo portátil).
- **Acceso Directo con Botón `Y`**: Al explorar las fuentes de un contenido, presiona **`Y` (Descargar)** para enviar el enlace a la cola.
- **Cola de Descargas Secuencial Inteligente**: Descarga los archivos de forma ordenada y a máxima velocidad para no saturar el bus ni fragmentar la tarjeta MicroSD FAT32.
- **Manejo Seguro de Archivos**:
  - Detección y resolución automática de colisiones de nombres (`Avatar (1).mkv`, `Avatar (2).mkv`).
  - Sincronización a disco con `fsync` y comprobación de integridad para evitar archivos incompletos o corruptos.
  - Pausa, reanudación y eliminación segura de descargas sin congelamientos ni crasheos.

#### 🏎️ 3. Optimizacion ante Congelamiento de UI
- **Subida de Texturas Pautada (*Paced Image Queue*)**: La carga de pósters a OpenGL se procesa dosificada (máximo 2 texturas por fotograma),mejorando la velocidad al desplazarse rápidamente por catálogos pesados con decenas de carruseles.
- **3 Hilos de Red Paralelos**: Carga simultánea y ultra-rápida de carátulas sin interferir con el hilo de dibujo.

#### ⚡ 4. CPU Boost Inteligente (`1785 MHz` FastLoad -> `1020 MHz` Normal)
- **FastLoad Automático (1785 MHz)**: Al abrir un torrent o stream, la consola eleva automáticamente su frecuencia a 1785 MHz para acelerar el inicio del búfer, los handshakes TCP y la verificación SHA-1 a la mitad de tiempo.
- **Normalización Dinámica (1020 MHz)**: Al iniciar la reproducción, la CPU regresa a su frecuencia base para mantener la consola fresca, silenciosa y optimizar la batería.

#### 🛠️ 5. Correcciones de Estabilidad y Crashes
- **Solución a Crashes al Adelantar / Buscar (`vd-lavc-dr=no`)**: Desactivación del Direct Rendering en libmpv para prevenir fallos de memoria DMA con decodificación por hardware en el chip Tegra X1.
- **Cambio Fluido de Pistas de Audio y Subtítulos**: Llamadas asíncronas con `mpv_command_async`, canal de audio continuo (`audio-stream-silence=yes`) y detección inteligente de búfer que elimina tirones o falsas alarmas de "Cargando búfer...".
- **HUD de Velocidad en Pantalla**: Indicador de velocidad de descarga en vivo en la esquina superior derecha (`📥 3.5 MB/s`).

---

### 🚀 Funcionalidades Principales Heredadas y Mejoradas

#### 🔗 1. Inicio de Sesión Rápido mediante Stremio Link (`https://link.stremio.com`)
- **Stremio Device Link**: Inicio de sesión en segundos escaneando el código QR o ingresando el código generado desde tu móvil o PC.
- **Sondeo Asíncrono (*Polling*)**: Detección automática al autorizar en la web, almacenando el token (`authKey`) de forma segura en la SD.
- **Login Manual Clásico**: Opción de inicio de sesión por correo y contraseña con teclado en pantalla Swkbd.

#### 🏠 2. Pestaña "Inicio" (Home) con Catálogos Dinámicos de Addons
- **Catálogos Unificados**: Muestra en carruseles horizontales las películas populares, series populares, películas destacadas, series destacadas y todos los catálogos personalizados que tengas instalados en tu cuenta de Stremio (Cyberflix, Netflix, Disney+, HBO Max, Prime Video, Anime, etc.).
- **Botón "Ver más" (*See More*)**: Abre cualquier catálogo en cuadrícula completa con desplazamiento infinito.
- **Carga Asíncrona Progresiva**: Los catálogos se dibujan en tiempo real sin bloquear la interfaz.

#### 🎬 3. Pestañas Dedicadas de Navegación
- **Continuar Viendo (*Continue Watching*)**: Seguimiento exacto de tus reproducciones en curso, barra de progreso y acceso con un toque al último capítulo visto.
- **Películas y Series**: Explorador categorizado por géneros y tendencias.
- **Biblioteca (*Library*)**: Sincronización completa con tus contenidos guardados en Stremio.
- **Búsqueda Global (*Search*)**: Búsqueda interactiva con teclado en pantalla.

#### 🎛️ 4. Tarjetas de Fuentes / Streams de 500px y Soporte de Emojis
- **Tarjetas Rediseñadas**:
  - Etiqueta superior con nombre del addon y calidad destacada (`[1080p] Torrentio`, `[RD+] Debrid`, etc.).
  - Título completo de la release con efecto de **marquesina automática (*auto-scroll*)** al enfocar.
  - Indicadores con número de semillas (seeders), tamaño de archivo y proveedor.
- **Fuente de Emojis Integrada (`OpenMoji.ttf`)**: Soporte nativo para visualizar banderas de idioma (🇪🇸, 🇲🇽, 🇬🇧, 🇯🇵) e iconos decorativos en los nombres de las fuentes.

#### 🚀 5. Soporte Completo para Streaming Debrid / HTTP / HTTPS
- Compatibilidad directa con enlaces de **Real-Debrid**, **AllDebrid**, **Torbox**, **Premiumize** y fuentes HTTP directas devueltas por addons.

#### 🧲 6. Motor BitTorrent Avanzado y Resolver de Magnets
- **MagnetResolver Inteligente**: Captura, procesa y deduplica automáticamente todos los trackers enviados por los addons (`stream.sources`).
- **Respaldo por Caché HTTP de Metadatos**: Consulta cachés públicas (`itorrents.org`, `btcache.me`) si los seeders P2P tardan en responder.

#### 🌐 7. Traducción Completa al Español y Configuración por Defecto
- Traducción integral al **Español (Latinoamérica y España)** de todos los menús, opciones, botones, diálogos y pistas.
- **Ajustes por defecto optimizados**:
  - Pestaña al Iniciar: **Stremio**
  - Idioma de Interfaz: **Español**
  - Idioma de Audio y Subtítulos: **Español**
  - Subtítulos al Iniciar: **Desactivados (OFF)**
  - Filtro: **Ocultar fuentes 4K Activado (ON)**

---

## 📥 Instalación

1. Descarga el archivo `NX-torrent-player.nro` desde los [Releases de GitHub](https://github.com/dl3g0/NX-torrent-player-mod/releases).
2. Cópialo en tu tarjeta microSD en la ruta `/switch/NX-torrent-player/NX-torrent-player.nro`.
3. Inicia la aplicación desde el **Homebrew Menu** en tu Nintendo Switch (preferiblemente en modo *Title Override* manteniendo `R` sobre un juego).

> **Actualizaciones**: La aplicación incluye un actualizador integrado en Ajustes que busca nuevas versiones automáticamente desde este repositorio.

---

## 🛠 Compilación desde el Código Fuente

Para compilar con **devkitPro / devkitA64**:

```bash
# Configuración con CMake
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake

# Compilación del .nro
make -j$(nproc) NX-torrent-player.nro
```

---

## 👏 Créditos y Agradecimientos

- **dl3g0** — Desarrollo y mantenimiento de este mod/fork.
- **shodowlo** — Creador del proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
- [borealis](https://github.com/xfangfang/borealis) — Framework UI estilo Horizon para Nintendo Switch.
- [mpv](https://mpv.io/) — Motor de reproducción multimedia de alto rendimiento.
- [libutp](https://github.com/bittorrent/libutp) — Transporte uTP para BitTorrent.
- [Stremio](https://www.stremio.com/) — Protocolo de cuentas, bibliotecas y addons.
- [OpenMoji](https://openmoji.org/) — Fuente de emojis de código abierto.
- [devkitPro](https://devkitpro.org/) / libnx — Cadena de herramientas de desarrollo para Nintendo Switch.
