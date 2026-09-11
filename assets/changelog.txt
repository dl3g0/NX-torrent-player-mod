# 🚀 NX Torrent Player MOD v0.0.8 — Release Notes

¡Bienvenidos a la versión **v0.0.8** de **NX Torrent Player (MOD)** desarrollada por **dl3g0**!

Esta actualización optimiza significativamente la fluidez y velocidad de respuesta al navegar por la aplicación, incorporando cancelación y reanudación inteligente de peticiones de red para catálogos y resolviendo la carga de miniaturas de capítulos en series de televisión.

---

## 🌟 Novedades Principales en v0.0.8

### 🛑 1. Cancelación Inteligente y Reanudación de Carga de Catálogos (Apertura Instantánea)
* **Apertura Inmediata**: Al seleccionar cualquier película o serie desde la pestaña de Inicio (o catálogo), las peticiones de catálogo que aún se estén descargando en segundo plano se cancelan y abortan de inmediato en milisegundos mediante tokens de cancelación atómicos en libcurl y tareas asíncronas.
* **Prioridad Total al Detalle**: La cola de red y los hilos de trabajo se liberan instantáneamente para que la información del contenido seleccionado (sinopsis, capítulos y enlaces) se descargue sin tener que esperar a que terminen de cargar las demás filas del catálogo.
* **Reanudación Automática y Fluida**: Al salir del detalle (presionar `B` o volver a la pestaña de Inicio), la descarga de los catálogos restantes se reanuda automáticamente donde se quedó, sin duplicar elementos ni perder la posición del foco ni el desplazamiento del usuario.

---

### 🖼️ 2. Corrección y Priorización en Miniaturas de Capítulos de Series
* **El Problema**: En las series, las imágenes y miniaturas de cada capítulo no cargaban en pantalla.
* **Causa Técnica**: La pausa del catálogo suspendía indebidamente la cola global de imágenes (`ImageQueue`) y el ciclo de subida de texturas a GPU (`processPendingImageUploads`), dejando congeladas las imágenes de los episodios.
* **Solución**: Se independizó la cola de imágenes del ciclo de pausa de catálogos y se implementó prioridad alta (`highPriority`) para las miniaturas de los episodios y el póster HD en las vistas de detalle, procesándolos inmediatamente al frente de la cola de descarga.

---

## 📥 Instalación

1. Descarga el archivo `NX-torrent-player.nro` adjunto en este release.
2. Cópialo a tu tarjeta microSD en:
   ```text
   sdmc:/switch/NX-torrent-player/NX-torrent-player.nro
   ```
3. Inicia la aplicación desde el **Homebrew Menu** en tu Nintendo Switch en modo **Title Override** (mantén presionado `R` sobre cualquier juego oficial instalado).

---

## 👏 Créditos y Agradecimientos

* **dl3g0** — Desarrollo, optimizaciones y arquitectura de este mod.
* **shodowlo** — Proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* [borealis](https://github.com/xfangfang/borealis), [mpv](https://mpv.io/), [HarfBuzz](https://harfbuzz.github.io/), [FriBidi](https://github.com/fribidi/fribidi), [libass](https://github.com/libass/libass), [libutp](https://github.com/bittorrent/libutp), [Stremio](https://www.stremio.com/), [OpenMoji](https://openmoji.org/) y devkitPro.
