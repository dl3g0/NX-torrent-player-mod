# 🚀 NX Torrent Player MOD v0.0.9.1 — Release Notes

¡Bienvenidos a la versión **v0.0.9.1** de **NX Torrent Player (MOD)** desarrollada por **dl3g0**!

Esta versión introduce un **Blindaje de Estabilidad Integral y Prevención de Cierres Inesperados (Anti-Crash)** tras una auditoría en la consola Nintendo Switch para erradicar cualquier vector de falla y garantizar un rendimiento 100% sólido.

---

## 🌟 Novedades en v0.0.9.1

### 🛡️ 1. Blindaje de Estabilidad Integral y Prevención de Cierres Inesperados (Anti-Crash)
* **Blindaje en Pestaña "Continuar" y Recarga (`(Y) Reload`)**:
  * Corregido el crasheo que ocurría al intercalar entre pestañas y presionar recargar en "Continuar viendo", o al iniciar la app e ir rápidamente a "Continuar" y recargar.
  * **Causa**: Al recargar o alternar vistas rápidamente, peticiones asíncronas de pósters intentaban actualizar tarjetas destruidas tras `clearViews()` debido a que `rowsAlive` no se invalidaba en `reload()`; además, `fetchLibraryAsync` modificaba la estructura global `g_libIds` concurrentemente en hilos secundarios generando colisiones de memoria, y el foco quedaba atrapado en elementos no enfocables si la lista no había terminado de cargar.
  * **Solución**: Se implementó un guardia de carga (`libLoading`) con identificador de secuencia (`libLoadSeq`), invalidación inmediata de `rowsAlive` en `reload()`, actualización segura de `g_libIds` exclusivamente en el hilo UI (`brls::sync`), estado visual de "Cargando..." cuando los datos aún están en vuelo para evitar estados vacíos prematuros, y un sistema de aparcado y rescate de foco en `finishList()` que redirige el cursor a la barra de pestañas superior si no hay tarjetas disponibles.
* **Aparcado de Foco en Descargas y Explorador de Archivos**:
  * Foco resguardado antes de vaciar y reconstruir listas en las actividades de descargas y explorador de archivos local.

---

## 📥 Instalación

1. Descarga el archivo `NX-torrent-player.nro` adjunto en este release.
2. Cópialo a tu tarjeta microSD en:
   ```text
   sdmc:/switch/NX-torrent-player/NX-torrent-player.nro
   ```
3. Inicia la aplicación desde el **Homebrew Menu** en tu Nintendo Switch (modo Title Override / manteniendo presionado `R` sobre cualquier juego).

---

## 👏 Créditos y Agradecimientos

* **dl3g0** — Desarrollo, modificaciones, correcciones y optimizaciones de este mod.
* **shodowlo** — Proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* [IntroDB](https://introdb.app/) — Base de datos comunitaria de marcas de tiempo de intros, recaps y créditos.
* [borealis](https://github.com/xfangfang/borealis), [mpv](https://mpv.io/), [libutp](https://github.com/bittorrent/libutp), [Stremio](https://www.stremio.com/), [OpenMoji](https://openmoji.org/) y devkitPro.
