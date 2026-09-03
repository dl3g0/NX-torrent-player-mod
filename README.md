<p align="center">
  <img src="https://github.com/shodowlo/NX-torrent-player/blob/main/assets/NX-torrent-player-banner.png?raw=true" alt="NX Torrent Player MOD" width="680">
</p>

# NX Torrent Player (MOD)

<p align="center">
  <strong>El cliente definitivo de Stremio, reproductor de Torrents P2P, Streams Debrid y Gestor de Descargas Offline para Nintendo Switch</strong>
</p>

<p align="center">
  <a href="https://github.com/dl3g0/NX-torrent-player-mod/releases"><img src="https://img.shields.io/github/v/release/dl3g0/NX-torrent-player-mod?style=flat-square&color=blue" alt="Latest Release"></a>
  <a href="https://github.com/dl3g0/NX-torrent-player-mod/blob/main/LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-green.svg?style=flat-square" alt="License"></a>
  <a href="https://github.com/dl3g0/NX-torrent-player-mod"><img src="https://img.shields.io/badge/Platform-Nintendo%20Switch-E60012?style=flat-square&logo=nintendoswitch&logoColor=white" alt="Nintendo Switch"></a>
  <a href="https://github.com/dl3g0/NX-torrent-player-mod"><img src="https://img.shields.io/badge/Maintained%20by-dl3g0-orange?style=flat-square" alt="Maintainer"></a>
</p>

---

## 📖 Descripción General del Proyecto

**NX Torrent Player (MOD)** es una suite multimedia integral de alto rendimiento diseñada específicamente para la consola **Nintendo Switch** con Custom Firmware (Atmosphère). 

Este proyecto parte como una evolución y reingeniería profunda del trabajo original de **shodowlo**, transformando el reproductor en un cliente completo de **Stremio**, reproductor multimedia acelerado por hardware con **libmpv**, motor de streaming **BitTorrent P2P / Debrid**, y un completo **gestor de descargas a la tarjeta MicroSD**.

Cuenta con una interfaz nativa fluida construida sobre el framework **Borealis**, adaptada a la estética Horizon OS de Nintendo Switch, con soporte para controles de mando, pantalla táctil, temas visuales y un rendimiento optimizado para el procesador Tegra X1.

> [!IMPORTANT]
> **Requisito Fundamental**: Se debe ejecutar siempre en modo **Title Override** (mantén presionado el botón **`R`** al abrir cualquier juego oficial instalado). Ejecutar la app desde el *Álbum* (modo applet) limita la memoria RAM disponible a unos pocos megabytes y causará cierres inesperados al procesar vídeo de alta definición o descargar contenidos.

---

## ✨ Características Principales

### 🌐 1. Ecosistema Stremio Completo y Sincronización en la Nube
* **Inicio de Sesión Ultrarrápido (`https://link.stremio.com`)**: Vincula tu cuenta en segundos escaneando un código QR desde el móvil o introduciendo el código de 4 caracteres en un navegador. También incluye inicio de sesión tradicional con correo y contraseña.
* **Sincronización Total con Móviles, PC y Web**:
  * **Continuar Viendo (*Continue Watching*)**: Lista idéntica a las aplicaciones oficiales de Stremio, ordenada con precisión por `lastWatched` descendente, mostrando barra de progreso exacta y reanudación inteligente de películas y episodios.
  * **Eliminación con Botón `X`**: Retira cualquier elemento de *Continuar Viendo* directamente con el botón `X`, actualizando los servidores de Stremio en tiempo real.
  * **Biblioteca en la Nube (*Library*)**: Todos tus contenidos guardados, series seguidas y películas favoritas sincronizadas al instante.
* **Pestaña Inicio (*Home*) con Catálogos de Addons**:
  * Películas y Series Populares / Destacadas vía Cinemeta.
  * Secciones dinámicas para cada addon instalado en tu cuenta (Cyberflix, Anime, Netflix, HBO Max, Disney+, Apple TV+, etc.).
  * Modo *"Ver Más"* con cuadrícula infinita y filtrado por categorías.
* **Búsqueda Global Integrada**: Encuentra cualquier título utilizando el teclado nativo en pantalla de la consola.

---

### 🎬 2. Motor de Reproducción de Alto Rendimiento (`libmpv`)
* **Decodificación por Hardware en Tegra X1**: Decodificación acelerada para códecs modernos (H.264, H.265 / HEVC 10-bit) a resolución nativa 720p (portátil) y 1080p (modo dock).
* **Reproductor Desacoplado para Archivos Locales (`LocalPlayerActivity`)**: Un motor exclusivo optimizado para reproducir archivos locales de la MicroSD y descargas finalizadas, libre de sobrecargas de red o búferes de torrent.
* **Salto Rápido Estilo Netflix/YouTube (*Multi-Step Seek*)**:
  * **Pantalla Táctil**: Doble toque en el lado derecho avanza **+10s** (`+10s ⏩`); en el lado izquierdo retrocede **-10s** (`⏪ 10s`). Toques continuos acumulan saltos mayores (**+20s**, **+30s**, **+40s**...). Un solo toque en el centro alterna los controles de inmediato.
  * **Gatillos Físicos `ZL` / `ZR`**: Doble pulsación rápida en **`ZR`** avanza +10s (acumulativo); doble pulsación en **`ZL`** retrocede -10s (acumulativo).
* **Ajuste Inteligente de Títulos Largos**: Los nombres extensos de películas o episodios se delimitan y desplazan suavemente (*marquee/auto-scroll*) sin sobreponerse al indicador de buffer ni a los botones de ajustes.
* **Menú de Configuración Rápida en Pantalla (`X`)**:
  * Selector de pistas de audio (multiidioma).
  * Selector de subtítulos incrustados y externos.
  * Ajuste de retardo de subtítulos (`sub-delay`) para sincronizar audio/subtítulos desfasados.
  * Modificador de velocidad de reproducción (0.5x hasta 2.0x).
  * **Panel de Diagnóstico Técnico**: Activa estadísticas avanzadas de red, mpv y torrents sin entorpecer la pantalla principal.
* **Navegación Fluida con Joystick Analógico**: Adelanta y retrocede de forma continua con aceleración progresiva y previsualización de tiempo sin congelar el vídeo.
* **Bloqueo de Controles (`Y`)**: Evita pulsaciones accidentales en la pantalla o botones durante la reproducción.

---

### 📴 3. Gestor de Descargas Offline a la MicroSD
* **Descargas en Segundo Plano**: Descarga películas o episodios completos a `sdmc:/switch/NX-torrent-player/downloads/` para disfrutarlos sin conexión (viajes, aviones o zonas sin Wi-Fi).
* **Acceso Directo con Botón `Y`**: En la lista de fuentes y enlaces de cualquier película o capítulo, pulsa **`Y` (Descargar)** para mandarlo a la cola de descargas.
* **Compatible con Enlaces Debrid y Torrents P2P**: Descarga directa a máxima velocidad desde tus servicios Debrid o directamente desde la red BitTorrent.
* **Cola de Descargas Secuencial**: Optimizado para el bus de almacenamiento de Nintendo Switch y sistemas de archivos FAT32/exFAT, evitando la fragmentación y asegurando transferencias fluidas.
* **Gestión Segura**: Renombrado automático ante duplicados, comprobación con `fsync` y opciones para pausar, reanudar o cancelar.

---

### 🚀 4. Soporte Integral Debrid & BitTorrent P2P
* **Compatibilidad Debrid de Alta Velocidad**: Enlaces premium instantáneos de **Real-Debrid**, **AllDebrid**, **Torbox**, **Premiumize** y URLs HTTP/HTTPS directas.
* **Reintento Inmediato de Streams (`Y`) y Detección de Timeout (15s)**:
  * Si un enlace falla, se corta o sufre lentitud excesiva, pulsa **`Y`** para reintentar la conexión al instante sin salir al menú.
  * Límite de espera inteligente a los 15 segundos para evitar cuelgues o pantallas de carga infinitas.
* **Motor BitTorrent Avanzado**:
  * Protocolo **uTP** para optimizar la latencia y atravesar NATs complicadas.
  * **MagnetResolver**: Captura y deduplica automáticamente los trackers de los addons (`stream.sources`).
  * **Caché HTTP de Metadatos**: Consulta rápida a cachés públicas (`itorrents.org`, `btcache.me`) para arrancar torrents sin esperar el handshake P2P.
  * **HUD de Estadísticas en Vivo**: Muestra semillas (seeds), pares activos (peers) y velocidad de transferencia en tiempo real (`📥 4.2 MB/s`).

---

### 🎨 5. Experiencia Visual y Diseño Cinematográfico
* **Fondos Panorámicos 16:9 Reales**: Reemplaza pósters estirados por fondos oficiales panorámicos de alta calidad cargados al instante desde la caché local sin parpadeos.
* **Logotipos Oficiales con Animación "Pulse"**: Al cargar un contenido, se presenta el logotipo oficial transparente del título con una suave animación de pulso senoidal (opacidad fluida entre `0.5` y `1.0`).
* **Pantallas de Carga y Error Rediseñadas**: Ocultación limpia de barras y porcentajes huérfanos ante fallos de conexión, mostrando mensajes claros y centrados con saltos de línea automáticos.
* **Tarjetas de Fuentes Rediseñadas (500px)**: Título completo con desplazamiento automático (*marquee auto-scroll*), etiqueta de calidad destacada (`[1080p] Torrentio`, `[RD+] Debrid`) y peso del archivo.
* **Fuente de Emojis Nativa (`OpenMoji.ttf`)**: Renderizado perfecto de banderas de idiomas (🇪🇸, 🇲🇽, 🇺🇸, 🇯🇵) e iconos en los nombres de las fuentes.

---

### ⚡ 6. Rendimiento y Optimización del Sistema
* **CPU FastLoad Boost (`1785 MHz` -> `1020 MHz`)**: Overclock seguro y dinámico que sube la CPU a 1785 MHz durante el arranque de búferes, handshakes TCP y hashes SHA-1 para iniciar la reproducción al instante, regresando a la frecuencia base (1020 MHz) para ahorrar batería.
* **Subida Pautada de Texturas (*Paced Texture Queue*)**: Limita la subida de imágenes a OpenGL (máximo 2 texturas por fotograma), evitando micro-congelamientos de la interfaz al desplazarse por catálogos densos.
* **Eliminación de Recargas Innecesarias**: Caché en memoria para transiciones instantáneas y fluidas entre pestañas (`Home`, `Continuar`, `Biblioteca`, `Búsqueda`).
* **Traducción Integral al Español**: Textos, menús, diálogos y descripciones adaptados al Español con configuraciones iniciales pensadas para la mejor experiencia.

---

## 📊 Tabla Comparativa: Versión Original vs. Versión MOD (dl3g0)

| Característica / Función | MOD (*dl3g0*) |
| :--- | :---: |
| **Inicio de Sesión Stremio Link (QR / Web)**| ✅ **QR + Código Web (`link.stremio.com`) y Email** |
| **Pestaña Inicio con Catálogos de Addons** | ✅ **Catálogos dinámicos completos** |
| **Lista "Continuar Viendo" Idéntica a Stremio** | ✅ **Orden oficial por `lastWatched` + Borrado con `X`** |
| **Gestor de Descargas Offline a MicroSD** | ✅ **Descargas Debrid / Torrents en segundo plano (`Y`)** |
| **Reproductor Local Exclusivo (`LocalPlayerActivity`)** | ✅ **Reproductor local desacoplado y optimizado** |
| **Salto Rápido Doble Toque Táctil y Gatillos `ZL` / `ZR`** | ✅ **Estilo Netflix/YouTube (+10s, +20s, +30s...)** |
| **Reintento Rápido de Streams (`Y`) y Anti-Timeout** | ✅ **Reconexión en 1 clic + Límite a los 15s** |
| **Fondos Panorámicos 16:9 + Logos con Pulso** | ✅ **Renderizado nativo sin parpadeos** |
| **CPU FastLoad Boost Inteligente** | ✅ **Dinámico: 1785 MHz carga -> 1020 MHz normal** |
| **Subida de Texturas Pautada (*Paced Image Queue*)** | ✅ **UI a 60 FPS estables** |
| **Soporte de Emojis / Banderas en Fuentes** | ✅ **Integración nativa de `OpenMoji.ttf`** |
| **Filtro Automático de Fuentes 4K** | ✅ **Opción activa por defecto para estabilidad Switch** |

---

## 🎮 Guía de Controles y Atajos

### 🧭 Navegación en Menús
* **`D-Pad` / `Stick Izquierdo`**: Moverse entre tarjetas, opciones y carruseles.
* **`A`**: Seleccionar / Abrir contenido.
* **`B`**: Volver atrás.
* **`L` / `R`**: Cambiar entre pestañas (*Inicio*, *Continuar*, *Biblioteca*, *Búsqueda*, *Local*).
* **`Y`**: En catálogos: recargar lista. En selector de fuentes: mandar a descargar a la MicroSD.
* **`X`**: En la pestaña *Continuar viendo*, elimina el título seleccionado del progreso en la nube.
* **`+` / `-`**: Abrir Ajustes de la aplicación.

### 🎬 Durante la Reproducción de Vídeo
* **`A`**: Pausar / Reanudar reproducción.
* **`ZL` (Doble pulsación rápida)**: Retroceso rápido acumulativo (**-10s**, **-20s**, **-30s**...).
* **`ZR` (Doble pulsación rápida)**: Avance rápido acumulativo (**+10s**, **+20s**, **+30s**...).
* **`Stick Izquierdo (Izquierda / Derecha)`**: Búsqueda continua en la línea de tiempo con previsualización.
* **`D-Pad Izquierda / Derecha`**: Salto de tiempo corto (10 segundos).
* **`L` / `R`**: Disminuir / Aumentar velocidad de reproducción.
* **`X`**: Abrir panel de opciones (audio, subtítulos, sincronización, velocidad y diagnóstico).
* **`Y`**: Durante la reproducción: Bloquear/Desbloquear controles. Durante la carga/error: Reintentar stream.
* **`B`**: Detener reproducción y salir al menú.
* **Pantalla Táctil**:
  * **Doble toque a la derecha**: Salto rápido hacia adelante (**+10s**, **+20s**, **+30s**...).
  * **Doble toque a la izquierda**: Salto rápido hacia atrás (**-10s**, **-20s**, **-30s**...).
  * **Toque simple en el centro**: Mostrar/Ocultar barra de controles y estado.
  * **Tocar la barra de progreso**: Saltar directamente a cualquier punto de la línea de tiempo.

---

## 📥 Instalación

1. Asegúrate de tener tu Nintendo Switch con Custom Firmware (Atmosphère).
2. Descarga la última versión del archivo `NX-torrent-player.nro` desde los [Releases de GitHub](https://github.com/dl3g0/NX-torrent-player-mod/releases).
3. Conecta la MicroSD a tu PC o mediante FTP/MTP y copia el archivo en la ruta:
   ```text
   sdmc:/switch/NX-torrent-player/NX-torrent-player.nro
   ```
4. En tu Nintendo Switch, entra a cualquier juego instalado manteniendo pulsado el botón **`R`** para abrir el Homebrew Menu en modo **Title Override**.
5. Abre **NX Torrent Player MOD** y ¡a disfrutar!

---

## 🛠️ Compilación desde el Código Fuente

El proyecto utiliza la cadena de herramientas oficial de **devkitPro (devkitA64)** y CMake.

### Requisitos:
* devkitPro instalado con `devkitA64`, `libnx` y las librerías portadas de Switch (`switch-mesa`, `switch-glad`, `switch-libdrm_nouveau`, `switch-curl`, `switch-mbedtls`, `switch-ffmpeg`, `switch-mpv`, etc.).

### Pasos para compilar:
```bash
# 1. Clonar el repositorio
git clone https://github.com/dl3g0/NX-torrent-player-mod.git
cd NX-torrent-player-mod

# 2. Crear y entrar en la carpeta de compilación
mkdir build && cd build

# 3. Configurar con CMake usando la toolchain de Switch
cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release

# 4. Compilar el archivo ejecutable .nro
cmake --build . --target NX-torrent-player.nro -j$(nproc)
```

El archivo resultante `NX-torrent-player.nro` se generará en la carpeta `build/`.

---

## 🤝 Créditos y Agradecimientos

* **[dl3g0](https://github.com/dl3g0)** — Desarrollo, optimizaciones, arquitectura y mantenimiento de este mod.
* **[shodowlo](https://github.com/shodowlo)** — Creador del proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* **[Borealis](https://github.com/xfangfang/borealis)** — Fantástico framework de interfaz gráfica estilo Nintendo Switch por *xfangfang*.
* **[mpv](https://mpv.io/)** — Motor multimedia libre y potente.
* **[libutp](https://github.com/bittorrent/libutp)** — Protocolo de transporte BitTorrent uTP.
* **[Stremio](https://www.stremio.com/)** — Protocolo de addons, cuentas y metadata.
* **[OpenMoji](https://openmoji.org/)** — Emojis de código abierto integrados.
* **[devkitPro](https://devkitpro.org/)** — Toolchain indispensable para el desarrollo Homebrew en Nintendo Switch.

---

<p align="center">
  <sub>Desarrollado con ❤️ para la comunidad homebrew de Nintendo Switch.</sub>
</p>
