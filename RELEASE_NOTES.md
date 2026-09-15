# 🚀 NX Torrent Player MOD v0.0.8 — Release Notes

¡Bienvenidos a la versión **v0.0.8** de **NX Torrent Player (MOD)** desarrollada por **dl3g0**!

---

## 🌟 Novedades Principales en v0.0.8

### ⚡ 1. Cancelación Inteligente y Reanudación de Catálogos (Apertura Instantánea)
* **Apertura Inmediata de Detalles**: Al entrar a la ficha técnica de una película o serie, se cancelan al instante las solicitudes y descargas de carátulas en segundo plano del catálogo principal. De esta forma, el ancho de banda y los núcleos del CPU se dedican 100% a la vista de detalle:
  * Prioridad 1: Renderizado inmediato del póster, título, sinopsis y metadatos.
  * Prioridad 2: Carga en segundo plano del logo y fondo de cabecera.
  * Prioridad 3: Consulta y listado de addons/streams de reproducción.
* **Reanudación Automática al Volver**: Al regresar del detalle de una serie o película hacia el catálogo, la carga del catálogo se reanuda o actualiza automáticamente sin bloqueos ni recargas innecesarias.

### 🖼️ 2. Corrección y Priorización en Miniaturas de Capítulos de Series
* **Solución a Miniaturas en Blanco**: Corregido el problema por el cual las imágenes de los episodios de las series no cargaban.
* **Carga Prioritaria de Episodios**: El gestor de miniaturas ahora prioriza las capturas de la temporada activa y los episodios visibles en pantalla, cancelando peticiones obsoletas al cambiar de temporada.

### 📑 3. Nueva Pestaña "Catálogos" en Opciones (Reordenar, Ocultar y Soporte Táctil)
* **Personalización Total de la Pantalla de Inicio**: Se agregó una nueva pestaña dedicada llamada **Catálogos** en la pantalla de Opciones (`X`):
  * **Lista Completa de Catálogos**: Visualiza todos los catálogos de Cinemeta (Películas Populares, Series Populares, Películas Destacadas, Series Destacadas) y los catálogos provistos por tus addons de Stremio.
  * **Soporte Táctil Nativo**:
    * Botones táctiles dedicados **▲** y **▼** en cada fila para subir y bajar catálogos directamente con los dedos en modo portátil.
    * Toca la etiqueta de estado (`Visible` / `Oculto`) para alternar la visibilidad de inmediato.
  * **Controles Tradicionales con Mando**:
    * **(A)**: Ocultar / Mostrar el catálogo enfocado.
    * **(X)**: Mover el catálogo hacia arriba.
    * **(Y)**: Mover el catálogo hacia abajo.
  * **Rendimiento Óptimo y Cero Bloqueos (Lazy Update)**:
    * Los cambios de visibilidad y orden se realizan de forma segura y diferida, evitando cierres inesperados (*crash*) al reordenar o cambiar estados en tiempo real.
    * Las modificaciones se aplican en lote de manera ultra rápida al salir del menú de Opciones, sin ralentizar la interfaz ni congelar la pantalla.
  * **Restablecer Orden y Visibilidad**: Botón al final de la lista para volver a la disposición por defecto en un solo toque o clic.
  * **Guardado Automático y Persistente**: Las preferencias se guardan en `config.json` y se conservan de forma permanente entre reinicios de la consola.

### 🔄 4. Sincronización Manual de Addons en la Vista de Cuenta
* **Nuevo Botón "Sincronizar Addons"**:
  * Ubicado en la pantalla de Cuenta junto al botón de cerrar sesión.
  * Permite actualizar al instante la colección de addons instalados desde los servidores de Stremio sin tener que reiniciar la aplicación ni cerrar sesión si se instalaron o modificaron addons desde otro dispositivo (PC, móvil o web).
  * Limpia la caché local de addons, actualiza el conteo, refresca la lista visual en pantalla y marca los catálogos para sincronizarse con la pantalla de inicio.

### 🌐 5. Internacionalización y Traducción Completa al 100%
* **Traducción de Botones en la Barra Inferior (Footer Hints)**:
  * Solucionado el problema por el cual botones como `OK` (Aceptar), `Back` (Atrás) o `Exit` (Salir) permanecían en inglés.
  * Se corrigió la sintaxis de recursos de Borealis (`hints.json`) y se integró un puente de traducción dinámico conectado directamente a `i18n::tr(...)`, garantizando que todos los botones de la barra inferior respeten siempre el idioma elegido en la aplicación sin importar el idioma del sistema operativo de la Switch.
* **Revisión Exhaustiva de Código**: Se auditó todo el código fuente de la aplicación para traducir cualquier texto restante en los 5 idiomas soportados:
  * 🇪🇸 **Español (Latinoamérica)** (`es`)
  * 🇪🇸 **Español (España)** (`es-es`)
  * 🇧🇷 **Português (Brasil)** (`pt-br`)
  * 🇷🇺 **Русский** (`ru`)
  * 🇫🇷 **Français** (`fr`)
  * 🇬🇧 **English** (Base)
* **Textos y Pantallas Traducidos**:
  * **Configuración y Ajustes**: Opciones de escalado de interfaz, límite de descarga en búfer (governor), notas de versión y actualización, créditos y enlaces oficiales del MOD.
  * **Temas y Colores**: Nombres de las paletas y esquemas de color (`Púrpura`, `Azul`, `Turquesa`, `Verde`, `Naranja`, `Rojo`, `Rosa`) traducidos y actualizados dinámicamente al cambiar de idioma.
  * **Catálogos y Géneros de Stremio**: Filtros de género traducidos en la interfaz (`Acción`, `Aventura`, `Animación`, `Comedia`, `Ciencia Ficción`, etc.) manteniendo la compatibilidad exacta con la API de Stremio.
  * **Reproductores Online y Offline**: Mensajes de velocidad en pantalla (`Speed 1.25x`), estados del búfer (`📥 Buffer: Xs`), botones de capítulos y selección de pistas.
  * **Pestaña de Descargas y Torrents**: Estados detallados de los archivos (`Descargado`, `Requerido`, `Descargando`, `Verificando`), etiquetas de progreso, errores de red y diálogos de confirmación.

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

* **dl3g0** — Desarrollo, modificaciones, correcciones y optimizaciones de este mod.
* **shodowlo** — Proyecto original [NX-torrent-player](https://github.com/shodowlo/NX-torrent-player).
* [borealis](https://github.com/xfangfang/borealis), [mpv](https://mpv.io/), [libutp](https://github.com/bittorrent/libutp), [Stremio](https://www.stremio.com/), [OpenMoji](https://openmoji.org/) y devkitPro.
