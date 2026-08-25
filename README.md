<img src="https://github.com/shodowlo/NX-torrent-player/blob/main/assets/NX-torrent-player-banner.png?raw=true" alt="NX Torrent Player MOD" width="640">

# NX Torrent Player (MOD)

**NX Torrent Player (MOD): Stremio y Reproductor de Torrents & Streams para Nintendo Switch**

Versión: **v0.0.1 (MOD by dl3g0)**  
Repositorio: [https://github.com/dl3g0/NX-torrent-player-mod](https://github.com/dl3g0/NX-torrent-player-mod)  
Basado en el proyecto original **NX Torrent Player** creado por **shodowlo**.

> Requiere una consola Nintendo Switch con Custom Firmware (Atmosphère).

---

## 🌟 Novedades y Modificaciones de este Fork vs Versión Original (por dl3g0)

Este mod transforma la aplicación original añadiendo nuevas funciones de inicio de sesión, rediseño de navegación, compatibilidad con Debrid y optimizaciones clave en el reproductor:

### 1. 🔗 Inicio de Sesión Rápido mediante Stremio Link (`https://link.stremio.com`)
- **Stremio Device Link**: Ahora puedes iniciar sesión en segundos sin necesidad de escribir tu correo y contraseña con el mando. La app genera un código de activación y una URL para autorizar la Switch desde el navegador de tu móvil o PC.
- **Sondeo Asíncrono (*Polling*)**: Detección automática al autorizar en la web, guardando el token de sesión (`authKey`) de forma segura en la SD.
- **Login Manual Clásico**: Se mantiene también el inicio de sesión por correo y contraseña con teclado Swkbd.

### 2. 🏠 Pestaña "Inicio" (Home) con Catálogos Dinámicos de Addons
- **Catálogos Unificados**: Muestra en carruseles horizontales las películas populares, series populares, películas destacadas, series destacadas y todos los catálogos personalizados que tengas instalados en tu cuenta de Stremio (Cyberflix, Netflix, Disney+, HBO Max, Prime Video, Anime, etc.).
- **Botón "Ver más" (*See More*)**: Permite abrir cualquier catálogo en vista de cuadrícula completa con desplazamiento infinito para explorar cientos de títulos.
- **Carga Asíncrona Progresiva**: Los catálogos se dibujan de manera no bloqueante en tiempo real; la app nunca se congela esperando respuestas de addons lentos.

### 3. 🎬 Pestañas Dedicadas y Corrección de Navegación
- **Pestaña Continuar Viendo (*Continue Watching*)**: Seguimiento exacto de tus reproducciones en curso, barra de progreso y acceso con un toque al último capítulo visto.
- **Pestañas Películas y Series**: Explorador categorizado por géneros y tendencias.
- **Pestaña Biblioteca (*Library*)**: Sincronización completa con tus contenidos guardados en Stremio.
- **Búsqueda Interactiva (*Search*)**: Búsqueda global de títulos con teclado en pantalla.
- **Corrección de Bug de Búsqueda**: Corregido el fallo donde la barra y resultados de búsqueda quedaban pegados en pantalla al regresar a la pestaña Inicio o alternar pestañas.

### 4. 🎛️ Tarjetas de Fuentes / Streams de 500px y Soporte de Emojis
- **Tarjetas Rediseñadas a 500px**:
  - Etiqueta superior con el nombre del addon y calidad destacada (`[1080p] Torrentio`, `[RD+] Debrid`, etc.).
  - Título completo de la versión/release con efecto de **marquesina automática (*auto-scroll*)** al enfocar con el cursor.
  - Indicadores secundarios con número de semillas (seeders), tamaño de archivo y proveedor.
- **Fuente de Emojis Integrada (`OpenMoji.ttf`)**: Soporte nativo para visualizar banderas de idioma (🇪🇸, 🇲🇽, 🇬🇧, 🇯🇵) e iconos decorativos (👤, 💾, 🎬, ⭐, 🔊, ⚙️) en los nombres de las fuentes.

### 5. 🚀 Soporte Completo para Streaming Debrid / HTTP / HTTPS
- Compatibilidad directa con enlaces de **Real-Debrid**, **AllDebrid**, **Torbox**, **Premiumize** y fuentes HTTP directas devueltas por addons.
- Resolución automática de redirecciones HTTP y optimización de búferes de red en `mpv`.

### 6. 📝 Motor de Subtítulos Optimizado y Estable
- **Extracción de Fuente Oficial de Switch**: Exporta la fuente compartida del sistema a `/switch/NX-torrent-player/subfont.ttf` para un renderizado nítido de subtítulos ASS/SSA y SRT.
- **Carga Diferida y Asíncrona**: Gestión fluida de archivos con más de 10-30 pistas de subtítulos incrustadas o subtítulos online (OpenSubtitles v3) sin congelar la app.
- **Eliminación de Bucle de Búfer**: Corregido el bloqueo de búfer infinito al cambiar entre pistas de subtítulos en reproducción.

### 7. 🧲 Mejoras en el Motor BitTorrent y Resolver de Magnets
- **MagnetResolver Inteligente**: Captura, procesa y deduplica automáticamente todos los trackers enviados por el addon (`stream.sources`).
- **Respaldo por Caché HTTP de Metadatos**: Si los seeders P2P tardan o no devuelven la metadata por BEP 9, el motor consulta automáticamente cachés públicas (`itorrents.org`, `btcache.me`, etc.) para comenzar la reproducción de inmediato.
- **Timeout de Conexión de 3s**: Mayor tolerancia para conexiones Wi-Fi y NATs residenciales.
- **Búfer Inicial Reducido a 5s**: Arranque de vídeo mucho más rápido.

### 8. 🌐 Traducción Completa al Español y Configuración por Defecto
- Traducción integral al **Español (Latinoamérica y España)** de todos los menús, opciones, botones, diálogos y pistas.
- **Ajustes por defecto optimizados para nueva instalación**:
  - Pestaña al Iniciar: **Stremio**
  - Idioma de Interfaz: **Español**
  - Idioma de Audio y Subtítulos: **Español**
  - Subtítulos al Iniciar: **Desactivados (OFF)**
  - Filtro: **Ocultar fuentes 4K Activado (ON)**

## 📥 Instalación

1. Descarga el archivo `NX-torrent-player.nro` desde los [Releases de GitHub](https://github.com/dl3g0/NX-torrent-player-mod/releases).
2. Cópialo en tu tarjeta microSD en la ruta `/switch/NX-torrent-player/NX-torrent-player.nro`.
3. Inicia la aplicación desde el **Homebrew Menu** en tu Nintendo Switch.

> **Actualizaciones**: La aplicación incluye un actualizador integrado que busca nuevas versiones automáticamente desde `https://github.com/dl3g0/NX-torrent-player-mod`.

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

- **dl3g0** — Desarrollo y mantenimiento de este mod/fork, inicio de sesión por Stremio Link, soporte Debrid, optimización de subtítulos, resolver de magnets, pestañas dinámicas de catálogo y traducciones.
- **shodowlo** — Creador del proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
- [borealis](https://github.com/xfangfang/borealis) — Framework UI estilo Horizon para Nintendo Switch.
- [mpv](https://mpv.io/) — Motor de reproducción multimedia de alto rendimiento.
- [libutp](https://github.com/bittorrent/libutp) — Transporte uTP para BitTorrent.
- [Stremio](https://www.stremio.com/) — Protocolo de cuentas, bibliotecas y addons.
- [OpenMoji](https://openmoji.org/) — Fuente de emojis de código abierto.
- [devkitPro](https://devkitpro.org/) / libnx — Cadena de herramientas de desarrollo para Nintendo Switch.
