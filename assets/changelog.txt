# 🚀 NX Torrent Player MOD v0.0.6 — Release Notes

¡Bienvenidos a la versión **v0.0.6** de **NX Torrent Player (MOD)** desarrollada por **dl3g0**!

Esta versión representa una de las actualizaciones más completas y pulidas hasta la fecha, introduciendo internacionalización total en 6 idiomas, soporte universal de subtítulos (incluyendo árabe y cirílico), prefetching gráfico sin pantallas negras, blindaje completo contra cierres inesperados por lentitud de red y una interfaz más limpia.

---

## 🌟 Novedades Principales en v0.0.6

### 🌐 1. Traducción Completa de la Aplicación (UI Internacional)
Se ha implementado la traducción integral de la interfaz de usuario en 6 idiomas seleccionables desde **Opciones / Ajustes**:
* **Español (Latino)**
* **Español (España)** *(Castellano adaptado: terminología, ajustes y ortografía)*
* **English**
* **Français**
* **Português (Brasil)**
* **Русский (Ruso)** *(Soporte completo de caracteres cirílicos en el sistema de menús)*
* **Cambio en tiempo real**: ¡Al seleccionar un nuevo idioma, la interfaz se actualiza instantáneamente en pantalla sin requerir reiniciar la consola ni la aplicación!

---

### 💬 2. Soporte Extendido de Subtítulos y Pistas de Audio
Detección, mapeo y selección automática prioritaria para subtítulos internos y complementos online (Stremio / OpenSubtitles):
* **Español (Latinoamérica)**: Detección inteligente de etiquetas `es-419`, `es-la`, `latino`, `es`, `spa`.
* **Español (España)**: Mapeo y prioridad para `es-es`, `es-ES`, `castellano`, `spa-es`.
* **English**: Códigos `en`, `eng`.
* **Français**: Códigos `fr`, `fre`, `fra`.
* **Português (Brasil)**: Soporte completo para etiquetas de add-ons `pob`, `pt-br`, `pt-BR`, `por-br`, `pb`.
* **Русский (Ruso)**: Detección de pistas y subtítulos `ru`, `rus`.
* **Árabe (Arabic)**: Detección y emparejamiento de subtítulos `ar`, `ara`.

---

### 🔤 3. Soporte Automático para Subtítulos en Árabe (Fuente Universal Integrada)
* La fuente estándar del sistema Nintendo Switch no incluye caracteres árabes por defecto.
* **Integración 100% Automática (Sin intervención del usuario)**:
  * Se ha integrado directamente dentro de la aplicación una fuente universal optimizada que abarca de forma completa todos los alfabetos: **Árabe**, **Cirílico (Ruso)** y **Latino extendido (Español, Portugués con tildes, Francés, Inglés)**.
  * La aplicación instala y gestiona esta fuente de manera automática e invisible en la tarjeta microSD en el primer inicio o actualización. **El usuario no necesita descargar ni mover ningún archivo**.
  * Los subtítulos en **árabe** ahora se reproducen perfectamente legibles y con fluidez desde el primer instante, eliminando cualquier caracter o cuadro vacío (`▯▯▯`).
  * *(Opcional para usuarios avanzados)*: Si se desea usar una tipografía externa específica, sigue siendo posible colocar un archivo `subfont.ttf` en `sdmc:/switch/NX-torrent-player/fonts/subfont.ttf`.

---

### 🖼️ 4. Carga Inmediata y Prefetching de Fondo (Backdrop) y Logo en el Reproductor
* **Visualización desde el primer intento**: Se corrigió el problema donde al ingresar por primera vez a un contenido la pantalla de carga ("Conectando a la transmisión...") permanecía en negro sin fondo ni logo.
* **Prefetching en segundo plano con alta prioridad**: Tanto el póster de fondo como el logo en PNG transparente se predescargan de manera prioritaria desde el momento en que se selecciona el título en la biblioteca o catálogo, estando ya listos en memoria al conectar al stream.
* **Transición fluida**: Se incluye respaldo visual instantáneo con el póster en caché y una duración mínima de presentación en la pantalla de carga para disfrutar del arte visual antes del inicio del vídeo.

---

### 🛡️ 5. Blindaje de Red y Apagado Seguro contra Cuelgues (Anti-Crash Shield)
* **Solución al cierre forzado con internet lento**: Se resolvió el error fatal (pantalla negra con código de error de Atmosphère) que ocurría al cerrar la aplicación o presionar HOME + Cerrar cuando la conexión a internet estaba congelada o lenta.
* **Mecanismo de cancelación inmediata (`http::abortAll()`)**: Al solicitar la salida del programa, todas las peticiones HTTP y transferencias activas se abortan en milisegundos.
* **Timeouts de conexión inteligentes**: Se configuraron tiempos máximos de conexión (10s) y velocidad mínima (15s) en libcurl para impedir que peticiones queden esperando minutos en bucle.
* **Cierre coordinado de trabajadores**: Los hilos en segundo plano (`stremio::shutdown()` y `download::shutdown()`) se detienen y unen de inmediato antes de que el sistema destruya los sockets BSD (`socketExit()`), garantizando una salida 100% limpia.

---

### 🎯 6. Barra de Pistas y Botón de Recarga (Y) Optimizados
* **Recarga contextual inteligente**: La acción `(Y) Recargar` ahora solo está visible y disponible en las pestañas **Continuar viendo** y **Biblioteca**, donde renueva el catálogo en vivo con animación de progreso integrada. En Inicio, Búsqueda y Local se oculta automáticamente.
* **Limpieza de indicadores duplicados**: Se eliminó el indicador duplicado de `Vista` en la barra inferior, consolidando la indicación limpia y elegante `[L] [R] Vista`.

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

* **dl3g0** — Desarrollo, arquitectura y optimizaciones de este mod/fork.
* **shodowlo** — Proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* [borealis](https://github.com/xfangfang/borealis), [mpv](https://mpv.io/), [libutp](https://github.com/bittorrent/libutp), [Stremio](https://www.stremio.com/), [OpenMoji](https://openmoji.org/) y devkitPro.
