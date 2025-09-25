# Wakefull

**Bloqueador de protector de pantalla simple para XFCE4**

Wakefull previene que XFCE4 active el protector de pantalla o ponga el sistema en suspensión. Es una alternativa simple y confiable a caffeine, optimizada específicamente para XFCE4.

## ¿Por qué Wakefull?

- ✅ **Diseñado para XFCE4**: Funciona nativamente con xfce4-power-manager
- ✅ **Simple y confiable**: Sin dependencias complejas, sin bloqueos
- ✅ **Múltiples métodos**: Usa varios enfoques para máxima compatibilidad
- ✅ **Modo daemon**: Se ejecuta en segundo plano de forma eficiente
- ✅ **Fácil de usar**: Solo 3 comandos principales

## Instalación

### Dependencias requeridas
```bash
# Ubuntu/Debian
sudo apt install gcc meson ninja-build

# Herramientas recomendadas para XFCE4
sudo apt install x11-xserver-utils xfconf
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

Wakefull usa múltiples métodos para mantener XFCE4 activo:

1. **xset**: Resetea el screensaver y mantiene DPMS activo
2. **xfconf-query**: Desactiva temporalmente configuraciones de XFCE4
3. **xdg-screensaver**: Como respaldo para compatibilidad
4. **Actividad del sistema**: Simula actividad periódica

### Para XFCE4 específicamente:
- Detecta automáticamente el entorno XFCE4
- Modifica temporalmente `xfce4-power-manager` y `xfce4-screensaver`
- Restaura configuraciones originales al detenerse
- Funciona sin interferir con el uso normal del sistema

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

**Wakefull v2.2.0** - Bloqueador de protector de pantalla simple y confiable para XFCE4