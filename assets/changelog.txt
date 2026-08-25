# NX Torrent Player MOD v0.0.1 — Notas de la Versión (Release Notes)

¡Bienvenidos a la primera versión de **NX Torrent Player (MOD)** creada por **dl3g0**!

Este mod es una reestructuración completa y optimización del proyecto original *NX Torrent Player* de *shodowlo*, añadiendo soporte integral para **Stremio Link**, streaming con servicios **Debrid**, un motor de subtítulos estable y libre de cuelgues, compatibilidad nativa con **Emojis**, navegación dinámica por catálogos y traducción completa al **Español**.

---

## 🚀 Principales Novedades y Mejoras

### 🔗 1. Inicio de Sesión Rápido por Stremio Link (`https://link.stremio.com`)
* **Código de Activación**: Inicia sesión en segundos escaneando o visitando `https://link.stremio.com` desde tu teléfono o PC. Ya no es necesario introducir tu correo y contraseña con los mandos de la Switch.
* **Sondeo Automático (*Polling*)**: La consola detecta la autorización en la nube al instante y guarda de forma segura tu token de sesión (`authKey`) en la tarjeta SD.
* **Acceso Manual Tradicional**: Se mantiene la opción de login clásico con correo y contraseña.

---

### 🏠 2. Rediseño de Navegación y Pestaña "Inicio" Unificada
* **Catálogos Dinámicos de Addons**: La pestaña **Inicio** reúne en carruseles horizontales las películas populares, series populares, destacados y todos los catálogos personalizados instalados en tu cuenta (Netflix, Disney+, HBO Max, Prime Video, Cyberflix, Anime, etc.).
* **Botón "Ver más" (*See More*)**: Permite abrir cualquier categoría en pantalla completa con cuadrícula paginada y scroll infinito.
* **Carga Asíncrona Progresiva**: Los catálogos se renderizan en tiempo real sin bloquear la interfaz ni congelar los FPS de la consola.
* **Pestaña Continuar Viendo**: Seguimiento en tiempo real del progreso de tus series y películas con barra de porcentaje y acceso directo al último capítulo reproducido.
* **Pestañas Películas, Series y Biblioteca**: Organización por categorías y sincronización con tu biblioteca en la nube.
* **Búsqueda Interactiva Sin Fallos**: Corregido el bug donde la barra de búsqueda quedaba pegada al cambiar entre pestañas.

---

### 🎛️ 3. Tarjetas de Fuentes / Streams de 500px y Soporte de Emojis
* **Tarjetas Ampliadas de 500px**:
  * Etiqueta de calidad y addon en color de acento (`[1080p] Torrentio`, `[RD+] Debrid`).
  * Título completo del release con efecto de **marquesina (*auto-scroll*)** al enfocar la tarjeta.
  * Indicadores de semillas (*seeders*), tamaño de descarga y proveedor.
* **Fuente de Emojis Nativa (`OpenMoji.ttf`)**: Soporte para banderas de idiomas (🇪🇸, 🇲🇽, 🇬🇧, 🇯🇵) e iconos decorativos (👤, 💾, 🎬, ⭐, 🔊, ⚙️).

---

### ⚡ 4. Soporte para Streaming Debrid / HTTP / HTTPS
* Reproducción fluida y directa de enlaces generados por **Real-Debrid**, **AllDebrid**, **Torbox**, **Premiumize**, etc.
* Resolución automática de redirecciones HTTP y configuración optimizada de búfer en red con `mpv`.

---

### 📝 5. Motor de Subtítulos Optimizado y Estable
* **Exportación de Fuente del Sistema**: Extrae automáticamente la fuente oficial de la Switch a `/switch/NX-torrent-player/subfont.ttf` para un renderizado nítido de subtítulos ASS/SSA y SRT.
* **Carga Asíncrona y Diferida**: Manejo eficiente de archivos con 10, 20 o más pistas de subtítulos incrustadas o subtítulos online (OpenSubtitles v3), evitando congelamientos y pausas al cambiar de pista.
* **Eliminación del Bucle de Re-búfer**: Corregido el bloqueo de buffer infinito al activar o alternar subtítulos durante la reproducción.

---

### 🧲 6. Mejoras en el Motor BitTorrent y Resolver de Magnets
* **MagnetResolver Inteligente**: Captura y deduplica automáticamente todos los trackers enviados por el addon (`stream.sources`).
* **Respaldo por Caché HTTP de Metadatos**: Consulta a servidores de caché pública (`itorrents.org`, `btcache.me`, etc.) si los seeders P2P tardan en responder por BEP 9.
* **Búfer Inicial de 5 Segundos**: Inicio de vídeo significativamente más rápido.
* **Timeout de Conexión de 3s**: Mayor tolerancia para conexiones Wi-Fi y NATs domésticos.

---

### 🌐 7. Traducción Integral al Español y Ajustes por Defecto
* **100% en Español**: Menús, diálogos, pistas de audio, menús de subtítulos y opciones traducidos al Español Latino / Castellano.
* **Configuración Predeterminada**:
  * Pestaña al Iniciar: **Stremio**
  * Idioma de Interfaz: **Español**
  * Idioma de Audio y Subtítulos Preferido: **Español**
  * Subtítulos al Iniciar: **Desactivados (OFF)**
  * Filtro: **Ocultar fuentes 4K Activado (ON)**

---

## 📥 Instalación

1. Descarga el archivo `NX-torrent-player.nro` adjunto en este release.
2. Cópialo a tu tarjeta microSD en la ruta:
   ```
   sdmc:/switch/NX-torrent-player/NX-torrent-player.nro
   ```
3. Abre el **Homebrew Menu** en tu Nintendo Switch e inicia **NX Torrent Player MOD**.

---

## 👏 Créditos

* **dl3g0** — Desarrollo y mantenimiento de este mod/fork.
* **shodowlo** — Proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* [borealis](https://github.com/xfangfang/borealis), [mpv](https://mpv.io/), [libutp](https://github.com/bittorrent/libutp), [Stremio](https://www.stremio.com/), [OpenMoji](https://openmoji.org/) y devkitPro.
