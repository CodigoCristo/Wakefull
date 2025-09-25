# Wakefull

**Bloqueador completo de protector de pantalla, suspensión y hibernación para XFCE4**

Wakefull previene que XFCE4 active el protector de pantalla, ponga el sistema en suspensión, hibernación o se suspenda al cerrar la tapa del laptop. Es una alternativa completa y confiable a caffeine, optimizada específicamente para XFCE4.

## ¿Por qué Wakefull?

- ✅ **Protección completa**: Previene protector de pantalla, suspensión, hibernación y cierre de tapa
- ✅ **Diseñado para XFCE4**: Funciona nativamente con xfce4-power-manager y xfce4-screensaver
- ✅ **Múltiples métodos**: xset, xfconf-query, systemd-inhibit, D-Bus y xdg-screensaver
- ✅ **Simple y confiable**: Sin dependencias complejas, sin bloqueos
- ✅ **Restauración automática**: Vuelve a la configuración original al detenerse
- ✅ **Modo daemon**: Se ejecuta en segundo plano de forma eficiente
- ✅ **Fácil de usar**: Solo 3 comandos principales

## Instalación

### Dependencias requeridas
```bash
# Ubuntu/Debian
sudo apt install gcc meson ninja-build

# Herramientas recomendadas para funcionalidad completa
sudo apt install x11-xserver-utils xfconf systemd dbus xdg-utils
```

### Compilar e instalar
```bash
# Clonar y compilar
git clone <repositorio>
cd Wakefull

# Compilar con meson
meson setup builddir
meson compile -C builddir
meson install -C builddir

# O usar el script de instalación
chmod +x install.sh
./install.sh
```

## Uso

### Comandos básicos
```bash
wakefull --start     # Iniciar daemon (bloquear protector de pantalla)
wakefull --stop      # Detener daemon
wakefull --status    # Ver estado actual
```

### Comandos adicionales
```bash
wakefull --foreground    # Ejecutar en primer plano (para debug)
wakefull --help         # Mostrar ayuda
wakefull --version      # Ver versión
```

### Ejemplos
```bash
# Uso típico
wakefull --start
# Realizar trabajo importante...
wakefull --stop

# Verificar que está funcionando
wakefull --status

# Debug (ver qué está haciendo)
wakefull --foreground
```

## Cómo funciona

Wakefull usa múltiples métodos simultáneos para protección completa:

### 1. **Control completo de XFCE4** (xfconf-query)
- Desactiva `xfce4-screensaver` temporalmente
- Configura `xfce4-power-manager` para prevenir:
  - Blank screen y DPMS
  - Suspensión por inactividad
  - Hibernación automática
  - Suspensión por cierre de tapa
- Modifica configuraciones de `xfce4-session`

### 2. **Inhibición universal** (systemd-inhibit)
- Bloquea: `sleep`, `idle`, `handle-lid-switch`, `handle-power-key`
- Funciona a nivel del sistema operativo
- Complementa las configuraciones de XFCE4

### 3. **Control de X11** (xset)
- Resetea screensaver continuamente
- Fuerza DPMS encendido
- Desactiva ahorro de energía de pantalla

### 4. **Métodos adicionales**
- **D-Bus**: Simula actividad de usuario y previene power management
- **xdg-screensaver**: Resetea screensaver como respaldo
- **Actividad del sistema**: Crea actividad periódica

### Restauración automática:
Al detenerse, wakefull restaura automáticamente todas las configuraciones originales de XFCE4, dejando el sistema como estaba antes.

## Archivos y logs

- **PID file**: `/tmp/wakefull.pid`
- **Log file**: `/tmp/wakefull.log`
- **Actividad**: `/tmp/.wakefull_keepalive`

## Solución de problemas

### Si el protector de pantalla sigue activándose:

1. **Verificar que está ejecutándose**:
   ```bash
   wakefull --status
   ```

2. **Revisar herramientas disponibles**:
   ```bash
   # Verificar que xset está instalado
   command -v xset && echo "✓ xset disponible"
   
   # Verificar que xfconf-query está instalado
   command -v xfconf-query && echo "✓ xfconf-query disponible"
   ```

3. **Ejecutar en modo debug**:
   ```bash
   wakefull --foreground
   ```

4. **Verificar log**:
   ```bash
   tail -f /tmp/wakefull.log
   ```

### Instalar herramientas faltantes:
```bash
# Ubuntu/Debian
sudo apt install x11-xserver-utils xfconf xdg-utils

# Fedora
sudo dnf install xorg-x11-server-utils xfce4-conf xdg-utils

# Arch Linux
sudo pacman -S xorg-xset xfconf xdg-utils
```

## Requisitos del sistema

- **Sistema operativo**: Linux
- **Entorno de escritorio**: XFCE4
- **Display server**: X11
- **Compilador**: GCC
- **Build system**: Meson + Ninja
- **Herramientas recomendadas**: systemd, xset, xfconf-query, dbus-send

## Funcionalidades bloqueadas

Wakefull previene **todas** estas acciones cuando está activo:

- ✅ **Protector de pantalla** (xfce4-screensaver)
- ✅ **Bloqueo automático de pantalla** por inactividad
- ✅ **Blank screen** (pantalla en negro)
- ✅ **DPMS** (apagado de monitor)
- ✅ **Suspensión del sistema** (sleep)
- ✅ **Hibernación** (hibernate)
- ✅ **Suspensión por cierre de tapa** en laptops
- ✅ **Suspensión por botón de power**
- ✅ **Suspensión por inactividad** (timeout)

## Desinstalar

```bash
# Si se instaló con meson
sudo ninja uninstall -C builddir

# Manual
sudo rm /usr/local/bin/wakefull
rm ~/.local/bin/wakefull  # si se instaló como usuario
```

## Contribuir

Este proyecto está enfocado específicamente en XFCE4. Las contribuciones son bienvenidas para:

- Mejorar compatibilidad con diferentes versiones de XFCE4
- Optimizar el rendimiento
- Corregir bugs específicos de XFCE4
- Mejorar documentación

1. Fork el repositorio
2. Crea un branch: `git checkout -b feature/mejora`
3. Commit: `git commit -m 'Agregar mejora'`
4. Push: `git push origin feature/mejora`
5. Crear Pull Request

## Licencia

GPL-3.0-or-later

---

**Wakefull v2.2.0** - Protección completa contra protector de pantalla, suspensión, hibernación y cierre de tapa para XFCE4