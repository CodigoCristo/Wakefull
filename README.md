# Wakefull

Mantén tu computadora despierta previniendo la inactividad del escritorio.

Wakefull es una implementación simple en C inspirada en el proyecto Caffeine. Previene que tu computadora entre en suspensión, active el salvapantallas o bloquee la pantalla inhibiendo la inactividad del escritorio.

## Repositorio

🔗 **GitHub**: https://github.com/CodigoCristo/Wakefull

## Características

- **Interfaz Simple**: Solo comandos `--start` y `--stop`
- **Ligero**: Escrito en C con dependencias mínimas
- **Seguro**: Maneja apropiadamente la limpieza y gestión de señales
- **Multiplataforma**: Funciona en cualquier entorno de escritorio Linux basado en X11

## Requisitos

- Linux con entorno de escritorio X11
- Librerías de desarrollo X11 (`libx11-dev`)
- Utilidad `xdg-screensaver` (usualmente parte de `xdg-utils`)
- Sistema de construcción Meson
- Herramienta de construcción Ninja
- Compilador de C (GCC o Clang)

## Instalación

### Instalación Rápida

```bash
git clone https://github.com/CodigoCristo/Wakefull
cd Wakefull
./install.sh
```

### Instalación Personalizada

Instalar en un prefijo personalizado:

```bash
./install.sh --prefix /usr
```

### Instalación Manual

Si prefieres construir manualmente:

```bash
# Instalar dependencias (Ubuntu/Debian)
sudo apt install meson ninja-build libx11-dev xdg-utils

# O en Arch Linux
sudo pacman -S meson ninja libx11 xdg-utils

# Construir e instalar
meson setup build
ninja -C build
sudo ninja -C build install
```

## Uso

### Iniciar prevención de inactividad del escritorio

```bash
wakefull --start
```

Esto hará:
- Crear un proceso daemon en segundo plano
- Crear una ventana X11 ficticia
- Usar `xdg-screensaver suspend` para prevenir la activación del salvapantallas
- Mantener funcionando en segundo plano hasta ser detenido

### Detener prevención de inactividad del escritorio

```bash
wakefull --stop
```

Esto hará:
- Reanudar el salvapantallas usando `xdg-screensaver resume`
- Limpiar el archivo de bloqueo y recursos X11

### Verificar estado

```bash
wakefull --status
```

Esto mostrará si wakefull está actualmente funcionando y mostrará el PID del daemon.

### Obtener ayuda

```bash
wakefull --help
```

## Cómo Funciona

Wakefull funciona:

1. Creando una ventana X11 mínima al iniciarse
2. Usando el ID de ventana con `xdg-screensaver suspend` para prevenir la detección de inactividad
3. Almacenando el ID de ventana en un archivo de bloqueo (`/tmp/wakefull.lock`)
4. Manteniendo el programa funcionando para mantener la conexión X11
5. Limpiando apropiadamente cuando se detiene o interrumpe

## Ejemplos

```bash
# Iniciar wakefull antes de ver una película
wakefull --start

# Verificar si está funcionando
wakefull --status

# Detener cuando termine
wakefull --stop

# Intentar iniciar nuevamente (mostrará mensaje "Ya funcionando")
wakefull --start
```

## Señales

Wakefull maneja apropiadamente estas señales para un apagado limpio:
- `SIGINT` (Ctrl+C)
- `SIGTERM` 
- `SIGHUP`

Al recibir cualquiera de estas señales, automáticamente reanudará el salvapantallas y limpiará los recursos.

## Archivos

- `wakefull.c` - Código fuente principal
- `meson.build` - Configuración de construcción Meson
- `install.sh` - Script de instalación
- `uninstall.sh` - Script de desinstalación
- `/tmp/wakefull.lock` - Archivo de bloqueo en tiempo de ejecución (contiene PID del daemon e ID de ventana)

## Diferencias con Caffeine

Aunque inspirado en Caffeine, Wakefull es más simple:

- **Sin GUI**: Interfaz solo de línea de comandos
- **Sin auto-detección**: Solo inicio/parada manual (sin detección automática de pantalla completa)
- **Sin bandeja del sistema**: Sin indicador o icono en la bandeja
- **Modo daemon**: Se ejecuta en segundo plano, no necesita mantener la terminal abierta
- **Dependencias mínimas**: Solo requiere X11 y xdg-utils

## Solución de Problemas

### Error "No se puede abrir display X11"
Asegúrate de estar ejecutando esto en un sistema con X11 y que la variable de entorno `DISPLAY` esté configurada.

### "xdg-screensaver no encontrado"
Instala el paquete xdg-utils:
```bash
# Ubuntu/Debian
sudo apt install xdg-utils

# Arch Linux  
sudo pacman -S xdg-utils
```

### El programa no previene la suspensión
Algunos entornos de escritorio o sistemas de gestión de energía pueden anular xdg-screensaver. Puede que necesites configurar las configuraciones de energía de tu entorno de escritorio específico.

## Licencia

Este proyecto está publicado bajo la Licencia Pública General GNU v3.0 o posterior.

## Contribuciones

¡Las contribuciones son bienvenidas! Por favor siéntete libre de enviar issues o pull requests.

## Reconocimientos

- Inspirado por el [proyecto Caffeine](https://launchpad.net/caffeine)
- Usa el mismo mecanismo subyacente (`xdg-screensaver`)