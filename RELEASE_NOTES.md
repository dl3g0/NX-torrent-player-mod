# 🚀 NX Torrent Player MOD v0.0.3 — Release Notes

¡Bienvenidos a la versión **v0.0.3** de **NX Torrent Player (MOD)** creada por **dl3g0**!

Esta gran actualización rediseña la experiencia visual con **fondos panorámicos 16:9** y **logotipos animados con pulso**, perfecciona la **sincronización en tiempo real de "Continuar Viendo"** con el orden oficial de Stremio, optimiza la fluidez eliminando recargas innecesarias y resuelve problemas críticos de estabilidad y cierres al salir.

---

## 🌟 Novedades Principales en v0.0.3

### 🎬 1. Sincronización Oficial de "Continuar Viendo" (*Continue Watching*)
* **Ordenamiento Oficial por `lastWatched`**:
  * La lista de reproducción en curso ahora coincide al 100% con las aplicaciones oficiales de Stremio (Android, PC y Web), ordenando estrictamente por la última fecha y hora de visualización.
  * Filtrado inteligente: requiere un `videoId` activo, progreso mayor a cero (`timeOffsetMs > 0`) y menor al 95%.
* **Eliminación Directa con Botón `X`**:
  * Permite eliminar cualquier película o serie de la lista de progreso con solo pulsar **`X` (Remove)**, actualizando la nube de Stremio y la memoria local en tiempo real sin recargar la pantalla.

---

### 🎨 2. Experiencia Visual Cinematográfica (Fondos 16:9 y Logos con Pulso)
* **Fondos Panorámicos Reales 16:9 (`background`)**:
  * Se sustituyeron las carátulas verticales deformadas por los fondos horizontales de alta resolución de Cinemeta / Metahub (`https://images.metahub.space/background/medium/{id}/img`).
  * Opacidad suave y cinematográfica integrada con el tema oscuro de la consola.
* **Logotipos Oficiales con Animación "Pulse"**:
  * En la pantalla de carga y reproducción de contenidos, se muestra el **logotipo oficial transparente** del título (`logo` de Metahub).
  * Nuevo componente `PulsingImage` con animación de opacidad senoidal fluida entre **0.50 y 1.00** para una experiencia visual moderna y premium.
* **Eliminación del Desenfoque por CPU (*CPU Blur Removal*)**:
  * Se eliminó el pesado algoritmo de desenfoque por software para acelerar la carga de la pantalla de reproducción y ahorrar memoria RAM.

---

### 🏎️ 3. Navegación Fluida Sin Recargas Innecesarias
* **Cero Parpadeos al Cambiar Pestañas (`L` / `R`)**:
  * Al desplazarse entre *Inicio*, *Continuar*, *Biblioteca* y *Búsqueda*, los contenidos se muestran inmediatamente desde la memoria RAM sin disparar descargas de red duplicadas.
  * Se eliminaron las triples reconstrucciones automáticas en segundo plano.
* **Subida de Texturas Pautada (*Paced Image Queue*)**:
  * Dosificación de carga a 2 texturas OpenGL por fotograma para mantener 60 FPS estables al desplazarse por catálogos extensos con decenas de carruseles.

---

### 🛠️ 4. Corrección de Cierres Inesperados al Salir y varias optimizaciones (*Crash Fix on Exit*)
* **Doble Liberación de Memoria Corregida (*Double Free Fix*)**:
  * Se solucionó un problema en `StremioTab::~StremioTab()` donde el destructor intentaba liberar vistas que ya habían sido eliminadas por el contenedor principal `libList`.
  * La aplicación ahora se cierra de forma instantánea y limpia al pulsar el botón Home o salir al menú principal.

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
