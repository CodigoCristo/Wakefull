# Wakefull

**Bloqueador de protector de pantalla y suspensión del sistema**

Wakefull previene que tu pantalla se apague, que el sistema entre en suspensión, hibernación o se suspenda al cerrar la tapa del laptop. Es la evolución de caffeine con capacidades superiores.

## ¿Por qué wakefull es mejor que caffeine?

| Característica | Caffeine | Wakefull |
|----------------|----------|----------|
| Protector de pantalla | ✅ | ✅ |
| Suspensión del sistema | ❌ | ✅ |
| Hibernación | ❌ | ✅ |
| Cierre de tapa | ❌ | ✅ |
| Wayland completo | ❌ | ✅ |
| Detección automática | ❌ | ✅ |
| Método XFCE4 nativo | ❌ | ✅ |
| Diagnóstico integrado | ❌ | ✅ |

## Comandos

```bash
wakefull --start     # Iniciar bloqueo (segundo plano)
wakefull --stop      # Detener bloqueo
wakefull --status    # Ver estado actual
wakefull --test      # Probar métodos disponibles
wakefull --diagnose  # Diagnosticar problemas del entorno
wakefull --help      # Mostrar ayuda
wakefull --version   # Ver versión
```

## Instalación

### Dependencias
```bash
# Ubuntu/Debian
sudo apt install meson ninja-build gcc libx11-dev systemd xdg-utils dbus

# Fedora
sudo dnf install meson ninja-build gcc libX11-devel systemd xdg-utils dbus

# Arch
sudo pacman -S meson ninja gcc libx11 systemd xdg-utils dbus
```

### Compilar e instalar
```bash
# Instalación automática
./install.sh                    # Para usuario actual
./install.sh --system           # Para todo el sistema (sudo)

# O manual
meson setup builddir
meson compile -C builddir
# El binario está en: builddir/wakefull
```

## Cómo funciona

Wakefull detecta automáticamente el mejor método disponible:

- **XFCE4-específico** (XFCE4): Configuración nativa de xfce4-power-manager y xfce4-screensaver
- **systemd-inhibit** (recomendado universal): Previene protector + suspensión + tapa
- **xdg-screensaver + xset**: Para X11, protector + power management  
- **D-Bus**: Alternativo para varios servicios de desktop

Se ejecuta como daemon en segundo plano hasta que lo detengas.

## Compatibilidad

### ✅ Funciona completamente
- **Distribuciones modernas** con systemd (Ubuntu, Fedora, Arch, etc.)
- **Escritorios principales**: GNOME, KDE, MATE, Cinnamon
- **XFCE4**: Soporte nativo optimizado con método específico
- **Gestores de ventana**: i3, Sway, bspwm, awesome, etc.
- **X11 y Wayland**

### ⚠️ Funcionalidad limitada
- **Sin systemd**: Solo prevención de protector de pantalla
- **Sistemas muy antiguos**: Soporte básico con xset
- **BSD**: Solo xdg-screensaver

## Uso

```bash
# Ejemplo típico
wakefull --start
# Hacer tu trabajo importante...
wakefull --stop

# En scripts
wakefull --start
./mi-tarea-larga.sh
wakefull --stop
```

## Solución de problemas

**Si no funciona:**
```bash
# 1. Diagnosticar el sistema y entorno
wakefull --diagnose

# 2. Verificar qué métodos están disponibles
wakefull --test

# 3. Instalar herramientas faltantes
# Ubuntu/Debian
sudo apt install systemd xdg-utils dbus

# Fedora/RHEL
sudo dnf install systemd xdg-utils dbus

# Arch/Manjaro
sudo pacman -S systemd xdg-utils dbus

# openSUSE
sudo zypper install systemd xdg-utils dbus-1

# Alpine (funcionalidad limitada)
sudo apk add xdg-utils dbus

# 4. Si sigue suspendiendo, verificar configuración de energía del sistema
```

**Casos específicos:**
- **XFCE4**: Wakefull incluye método nativo que configura automáticamente xfce4-power-manager y xfce4-screensaver
- **GNOME**: Deshabilitar "Blank screen" en Privacy > Screen Lock
- **KDE**: Revisar Energy Saving settings  
- **Wayland sin systemd**: Funcionalidad muy limitada

**Para XFCE4 específicamente:**
- Wakefull detecta XFCE4 automáticamente y usa método optimizado
- Modifica temporalmente configuraciones del power manager
- Restaura configuraciones originales automáticamente al parar
- Si tienes problemas: `wakefull --diagnose` para ver configuraciones actuales

## Contribuir

1. Fork el proyecto
2. Crea tu branch: `git checkout -b mi-feature`
3. Commit: `git commit -am 'Agregar feature'`
4. Push: `git push origin mi-feature`
5. Crear Pull Request

## Licencia

GPL-3.0-or-later

---

**Wakefull v2.1.1** - Previene protector de pantalla, suspensión, hibernación y cierre de tapa

### Características destacadas v2.1.1
- **Soporte nativo para XFCE4**: Método específico que maneja xfce4-power-manager directamente
- **Diagnóstico integrado**: `--diagnose` para identificar problemas del entorno
- **Configuración automática**: En XFCE4 se configuran y restauran ajustes temporalmente
- **Detección mejorada**: Reconoce automáticamente el mejor método por entorno