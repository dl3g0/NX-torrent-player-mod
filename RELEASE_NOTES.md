# 🚀 NX Torrent Player MOD v0.0.5 — Release Notes

¡Bienvenidos a la versión **v0.0.5** de **NX Torrent Player (MOD)** desarrollada por **dl3g0**!

---

## 🌟 Novedades Principales en v0.0.5

### 📁 1. Nuevo Explorador de Archivos de la MicroSD (`File Browser`)
* **Navegación Completa por la MicroSD**:
  * Accede a cualquier directorio de tu tarjeta de memoria desde el nuevo botón **`📁 Explorador`** en la pestaña **LOCAL**.
  * Navega libremente por cualquier carpeta (`sdmc:/`) con iconos dedicados (`📁`) y previsualización de archivos multimedia.
* **Compatibilidad Universal de Formatos**:
  * Soporte y detección automática para reproducir vídeos en formatos: `.mkv`, `.mp4`, `.avi`, `.ts`, `.webm`, `.mov`, `.m4v`, `.wmv`, `.flv` y archivos `.torrent`.
* **Navegación Ágil con Botón `B` y Memoria de Ruta**:
  * Presionar **`B`** (o seleccionar `📁 .. (Carpeta superior)`) sube un nivel en el árbol de directorios; al llegar a la raíz `sdmc:/`, regresa a la pestaña principal.
  * La aplicación recuerda la última carpeta explorada durante la sesión para que continúes exactamente donde estabas.
* **Reproducción Directa con Hardware Acceleration**:
  * Al pulsar **`A`** sobre cualquier vídeo, arranca de inmediato con decodificación completa por hardware NVDEC en 720p/1080p sin depender de la carpeta de descargas de la app.

### ⚙️ 2. Mejoras de Compilación y Empaquetado
* **Generación Automática del NRO**: Se configuró el objetivo `ALL` en CMake para que cada compilación empaquete y actualice siempre el archivo `NX-torrent-player.nro` con su RomFS de forma inmediata.

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
