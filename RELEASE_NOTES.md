# 🚀 NX Torrent Player MOD v0.0.9.2 — Release Notes

¡Bienvenidos a la versión **v0.0.9.2** de **NX Torrent Player (MOD)** desarrollada por **dl3g0**!

Esta versión introduce la **Traducción Integral al 100% de la Interfaz en Griego (Ελληνικά)**, **Soporte Nativo Completo para Subtítulos y Audio en Griego**, una **Ampliación del Límite de Subtítulos Online (de 40 a 120 pistas)** para evitar la exclusión de idiomas internacionales en títulos populares, y consolida todos los blindajes de estabilidad y mejoras introducidas recientemente.

---

## 🌟 Novedades en v0.0.9.2

### 🇬🇷 1. Interfaz de Usuario 100% en Griego (Ελληνικά)
* **7º Idioma Oficial del Sistema**:
  * Ahora el **Griego (`Ελληνικά`)** se une a la lista de idiomas del sistema en `Ajustes -> Opciones -> Idioma`.
  * **Traducción 100% Completa**: Más de 560 cadenas de texto traducidas rigurosamente con terminología nativa para abarcar la totalidad de la aplicación: menús, opciones, descargas offline, explorador local de archivos, biblioteca de Stremio, reproductor y diálogos del sistema.
* **Indicadores Contextuales (Borealis Hints)**:
  * Los botones de acción de la barra inferior y cuadros de diálogo (`Aceptar`, `Atrás`, `Salir`, etc.) ahora se presentan de forma nativa en griego (`Εντάξει`, `Πίσω`, `Έξοδος`, `Ακύρωση`, `Υποβολή`).
* **Soporte Tipográfico Completo**:
  * Renderizado nativo del alfabeto y acentos griegos (`ά, έ, ή, ί, ό, ύ, ώ`) en la interfaz de usuario mediante las fuentes compartidas del sistema Nintendo Switch y cobertura total en subtítulos con la tipografía universal `FreeSans`.

---

### 🌐 2. Soporte Nativo de Subtítulos y Audio en Griego + Expansión a 120 Pistas
* **Integración del Idioma Griego en Ajustes**:
  * Ahora es posible seleccionar **Griego (Greek)** en `Ajustes -> Reproducción -> Idioma de subtítulos` y `Idioma de audio`.
  * **Auto-selección inteligente (*Sticky Subtitles*)**: Al reproducir cualquier contenido, el reproductor prioriza y activa automáticamente los subtítulos en griego si están disponibles, sin necesidad de seleccionarlos manualmente en cada vídeo.
  * Mapeo transparente de todos los estándares ISO 639-1, 639-2 y 639-3 (`el`, `ell`, `gre`, `greek`), cubriendo tanto pistas internas de archivos MKV/MP4 como las respuestas de OpenSubtitles y addons comunitarios de Stremio.
* **Ampliación del Límite de Subtítulos en Stremio (`kMaxSubs`: 40 ➔ 120)**:
  * Anteriormente, la consulta a OpenSubtitles limitaba los resultados a un máximo de 40 archivos. En películas y series de alta popularidad con docenas de variantes en inglés, español y portugués, los subtítulos de idiomas secundarios e internacionales (griego, turco, polaco, rumano, etc.) quedaban recortados y descartados antes de llegar al reproductor.
  * El nuevo límite de 120 pistas garantiza que todas las opciones de idioma estén siempre disponibles en el menú de pistas (`X`).
* **Soporte Tipográfico en Subtítulos**:
  * Renderizado impecable de todo el alfabeto griego (`α, β, γ, δ...`) en pantalla mediante la fuente universal integrada `FreeSans` (UTF-8).
* **Traducción Dinámica de Pistas**:
  * Las pistas de subtítulos en línea ahora se traducen contextualmente en el menú según el idioma de la consola (ej. *"Griego - OpenSubtitles v3"* en español, *"Greek - OpenSubtitles v3"* en inglés, *"Ελληνικά - OpenSubtitles v3"* en griego).
* **Ampliación del Rango de Sincronización de Subtítulos (±10 s ➔ ±120 s)**:
  * El límite anterior de 10 segundos impedía compensar los desfases comunes entre versiones WEB-DL y Blu-Ray producidos por intros de distribuidoras o resúmenes ("Previously on..."), donde idiomas como el griego sufren desajustes de 15 a 45 segundos.
  * Se amplió el rango máximo de sincronización hasta **±120 segundos (2 minutos completos)**.
  * **Nuevos atajos de control en el panel de reproducción**:
    * **L / R**: Ajuste fino de precisión de **±0.1 segundos**.
    * **ZL / ZR**: Salto rápido de **±1.0 segundo** (con repetición acelerada al mantener presionado).
    * **Botón (Y)**: Restablecimiento instantáneo del desfase a cero (`None` / 0.00 s).



---

### 🛡️ 3. Corrección de Errores y Blindaje de Estabilidad
* **Traducción Integral de la Barra Inferior en Todos los Idiomas (Footer Hints)**:
  * Resuelto el problema donde botones de la barra inferior quedaban sin traducir o fijos en español/inglés (mostrando *"Salir"* en lugar de *"Έξοδος"* / *"Exit"* / *"Выход"* o *"ok"* en lugar de *"Εντάξει"* / *"Aceptar"* / *"ОК"*).
  * Se implementó un mapeo multidireccional en el hook de traducción de Borealis que normaliza y traduce dinámicamente todas las acciones de pie de página (`OK`, `Atrás`, `Salir`, `Cancelar`, `Eliminar`, `Recargar`, `Vista`, etc.) en los 7 idiomas disponibles, actualizándose al instante al cambiar de idioma en Ajustes.
* **Solución al Bloqueo de la Pestaña "Inicio"**:
  * Corregido un problema donde al alternar entre las pestañas *"Continuar"* o *"Biblioteca"* y luego intentar regresar a *"Inicio"*, presionar el botón *"Inicio"* no hacía nada y no cargaba la vista.
  * Ahora el cambio de pestañas reactiva el estado de carga y refresca de forma consistente el árbol de vistas (`renderView`), restaurando el foco y las tiras de contenido sin omitir el renderizado.
* **Blindaje Total al Cambiar de Idioma al Iniciar la App**:
  * Resuelto el cierre crítico (*crash*) que ocurría cuando el usuario ingresaba a la aplicación y cambiaba de idioma inmediatamente en Ajustes.
  * Se sustituyó el ciclo de destrucción y reapertura de actividades (`popActivity`/`pushActivity`) por una recarga directa *in-place* de la vista (`setContentView(createContentView())`), preservando la pila de navegación intacta y eliminando colisiones con la animación de salida del desplegable.
  * Se invalidaron de inmediato los tokens de ciclo de vida (`rowsAlive`), bloqueando cualquier acceso de callbacks de pósters pendientes a vistas de carátulas ya destruidas (*use-after-free*).
  * Se postergó la regeneración de catálogos de Stremio mientras la pantalla de Opciones permanece activa (`markHomeCatalogsDirty()`), previniendo el secuestro de foco hacia la ventana de fondo y refrescando la pantalla principal con total fluidez al regresar.
  * Se dotó a `i18n` de protección mediante `std::mutex` en lecturas y cargas (`load()` y `tr()`), blindando accesos concurrentes de hilos secundarios (como la comprobación de actualizaciones).
* **Prevención de Cierres Inesperados (Crash) al Retroceder Rápidamente**:
  * Solucionado un cierre repentino (*"Se ha cerrado el programa a causa de un error"*) que ocurría cuando el usuario entraba rápidamente al detalle de una película o serie y pulsaba **Atrás (`B`)** mientras las peticiones asíncronas de fuentes, streams o carátulas HQ aún estaban en vuelo.
  * Se implementó la invalidación inmediata de ciclo de vida (`willDisappear`) para cancelar llamadas en curso antes de la animación de cierre, evitando accesos a memoria destruida (*use-after-free*) y postergando la reanudación de catálogos (`resumeCatalogLoads`) al siguiente ciclo del hilo principal.

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
