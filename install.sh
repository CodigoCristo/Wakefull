#!/bin/bash

# install.sh - Instalador para wakefull
# Bloqueador simple de protector de pantalla

set -e

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROGRAM_NAME="wakefull"
VERSION="2.1.0"

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1" >&2
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_header() {
    echo -e "${BLUE}=== Wakefull Installer ${VERSION} ===${NC}"
    echo -e "${BLUE}=== Bloqueador simple de protector de pantalla ===${NC}"
    echo ""
}

# Detectar si estamos en XFCE4
is_xfce4() {
    local desktop=$(echo "${XDG_CURRENT_DESKTOP:-}" | tr '[:upper:]' '[:lower:]')
    local session=$(echo "${DESKTOP_SESSION:-}" | tr '[:upper:]' '[:lower:]')

    if [[ "$desktop" == *"xfce"* ]] || [[ "$session" == *"xfce"* ]]; then
        return 0
    fi

    # Verificar si xfconf-query funciona
    if command -v xfconf-query >/dev/null 2>&1; then
        if xfconf-query -c xfce4-session -l >/dev/null 2>&1; then
            return 0
        fi
    fi

    return 1
}

# Verificar dependencias de build
check_build_deps() {
    print_info "Verificando dependencias de compilación..."

    local missing=0

    # Verificar meson
    if command -v meson >/dev/null 2>&1; then
        print_success "Meson encontrado: $(meson --version)"
    else
        print_error "Meson no encontrado"
        print_info "Instala con: sudo apt install meson (Ubuntu/Debian)"
        print_info "O: sudo dnf install meson (Fedora)"
        print_info "O: sudo pacman -S meson (Arch)"
        missing=$((missing + 1))
    fi

    # Verificar ninja
    if command -v ninja >/dev/null 2>&1; then
        print_success "Ninja encontrado"
    else
        print_error "Ninja no encontrado"
        print_info "Instala con: sudo apt install ninja-build (Ubuntu/Debian)"
        print_info "O: sudo dnf install ninja-build (Fedora)"
        print_info "O: sudo pacman -S ninja (Arch)"
        missing=$((missing + 1))
    fi

    # Verificar GCC
    if command -v gcc >/dev/null 2>&1; then
        print_success "GCC encontrado"
    else
        print_error "GCC no encontrado"
        print_info "Instala con: sudo apt install build-essential"
        missing=$((missing + 1))
    fi

    if [ $missing -gt 0 ]; then
        print_error "Faltan $missing dependencias de compilación"
        exit 1
    fi

    print_success "Dependencias de compilación verificadas"
    echo ""
}

# Verificar herramientas de runtime
check_runtime_deps() {
    print_info "Verificando herramientas de inhibición..."

    local methods=0
    local xfce4_detected=false

    # Detectar XFCE4
    if is_xfce4; then
        xfce4_detected=true
        print_success "XFCE4 detectado"

        if command -v xfconf-query >/dev/null 2>&1; then
            print_success "xfconf-query disponible (método XFCE4 específico habilitado)"
            methods=$((methods + 1))
        else
            print_warning "xfconf-query no disponible - instala: sudo apt install xfconf"
        fi
    fi

    if command -v systemd-inhibit >/dev/null 2>&1; then
        if [ "$xfce4_detected" = true ]; then
            print_success "systemd-inhibit disponible (respaldo universal)"
        else
            print_success "systemd-inhibit disponible (recomendado)"
        fi
        methods=$((methods + 1))
    fi

    if command -v xdg-screensaver >/dev/null 2>&1; then
        print_success "xdg-screensaver disponible"
        methods=$((methods + 1))
    fi

    if command -v dbus-send >/dev/null 2>&1; then
        print_success "dbus-send disponible"
        methods=$((methods + 1))
    fi

    # Recomendaciones específicas para XFCE4
    if [ "$xfce4_detected" = true ]; then
        echo ""
        print_info "Recomendaciones para XFCE4:"
        print_info "• Wakefull incluye método específico optimizado para XFCE4"
        print_info "• Se configurará automáticamente para máxima compatibilidad"
    fi

    if [ $methods -eq 0 ]; then
        print_warning "No hay métodos de inhibición disponibles"
        print_info "Instala: sudo apt install systemd xdg-utils dbus"
        if [ "$xfce4_detected" = true ]; then
            print_info "Para XFCE4 específicamente: sudo apt install xfconf"
        fi
    else
        print_success "$methods método(s) de inhibición disponible(s)"
    fi
    echo ""
}

# Compilar con Meson
build_program() {
    print_info "Configurando proyecto con Meson..."

    # Limpiar build anterior si existe
    if [ -d "builddir" ]; then
        rm -rf builddir
    fi

    # Setup build
    if meson setup builddir; then
        print_success "Proyecto configurado"
    else
        print_error "Fallo al configurar el proyecto"
        exit 1
    fi

    print_info "Compilando..."

    # Compile
    if meson compile -C builddir; then
        print_success "Compilación completada"
    else
        print_error "Fallo al compilar"
        exit 1
    fi

    # Verificar que el binario existe
    if [ -f "builddir/wakefull" ]; then
        print_success "Binario creado exitosamente"
    else
        print_error "No se encontró el binario compilado"
        exit 1
    fi

    echo ""
}

# Probar el programa
test_program() {
    print_info "Probando programa..."

    local binary="builddir/wakefull"

    if $binary --version >/dev/null 2>&1; then
        print_success "Comando --version funciona"
    else
        print_error "Comando --version falló"
        return 1
    fi

    if $binary --test >/dev/null 2>&1; then
        print_success "Detección de métodos funciona"
    else
        print_error "Detección de métodos falló"
        return 1
    fi

    print_success "Pruebas básicas completadas"
    echo ""
}

# Instalar para usuario
install_user() {
    local install_dir="$HOME/.local/bin"

    print_info "Instalando para usuario en $install_dir..."

    mkdir -p "$install_dir"

    if cp builddir/wakefull "$install_dir/"; then
        chmod 755 "$install_dir/wakefull"
        print_success "Instalado en $install_dir/"
    else
        print_error "Fallo al copiar el binario"
        return 1
    fi

    # Verificar PATH
    if echo "$PATH" | grep -q "$install_dir"; then
        print_success "$install_dir está en tu PATH"
    else
        print_warning "$install_dir no está en tu PATH"
        print_info "Agrega a tu ~/.bashrc:"
        print_info "export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi

    return 0
}

# Instalar para sistema
install_system() {
    local install_dir="/usr/local/bin"

    print_info "Instalando para todo el sistema en $install_dir..."

    if [ "$EUID" -ne 0 ]; then
        print_error "Instalación del sistema requiere privilegios de root"
        print_info "Ejecuta: sudo $0 --system"
        return 1
    fi

    if cp builddir/wakefull "$install_dir/"; then
        chmod 755 "$install_dir/wakefull"
        print_success "Instalado en $install_dir/"
    else
        print_error "Fallo al copiar el binario"
        return 1
    fi

    return 0
}

# Desinstalar
uninstall() {
    print_info "Desinstalando wakefull..."

    local locations=(
        "/usr/local/bin/wakefull"
        "/usr/bin/wakefull"
        "$HOME/.local/bin/wakefull"
    )

    local found=0
    for location in "${locations[@]}"; do
        if [ -f "$location" ]; then
            if rm "$location" 2>/dev/null; then
                print_success "Eliminado: $location"
                found=$((found + 1))
            else
                print_error "No se pudo eliminar: $location"
            fi
        fi
    done

    if [ $found -eq 0 ]; then
        print_warning "No se encontraron instalaciones"
    else
        print_success "Desinstalado $found instancia(s)"
    fi

    # Limpiar archivos temporales
    rm -f /tmp/wakefull.* 2>/dev/null || true
    print_success "Archivos temporales limpiados"
}

# Mostrar uso
show_usage() {
    echo "Uso: $0 [OPCIÓN]"
    echo ""
    echo "Opciones:"
    echo "  --user          Instalar para usuario actual (por defecto)"
    echo "  --system        Instalar para todo el sistema (requiere sudo)"
    echo "  --uninstall     Desinstalar"
    echo "  --test-only     Solo compilar y probar"
    echo "  --check-deps    Solo verificar dependencias"
    echo "  --clean         Limpiar archivos de compilación"
    echo "  --help          Mostrar esta ayuda"
    echo ""
    echo "Ejemplos:"
    echo "  $0                # Compilar e instalar para usuario"
    echo "  $0 --system       # Instalar para todo el sistema"
    echo "  $0 --test-only    # Solo compilar y probar"
    echo "  $0 --clean        # Limpiar build"
}

# Limpiar archivos de build
clean_build() {
    print_info "Limpiando archivos de compilación..."

    if [ -d "builddir" ]; then
        rm -rf builddir
        print_success "Directorio builddir eliminado"
    fi

    print_success "Limpieza completada"
}

# Función principal
main() {
    local install_type="user"
    local test_only=false
    local check_deps_only=false

    # Parsear argumentos
    while [ $# -gt 0 ]; do
        case $1 in
            --user)
                install_type="user"
                ;;
            --system)
                install_type="system"
                ;;
            --uninstall)
                print_header
                uninstall
                exit 0
                ;;
            --test-only)
                test_only=true
                ;;
            --check-deps)
                check_deps_only=true
                ;;
            --clean)
                clean_build
                exit 0
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Opción desconocida: $1"
                show_usage
                exit 1
                ;;
        esac
        shift
    done

    print_header

    # Verificar dependencias
    check_build_deps
    check_runtime_deps

    if [ "$check_deps_only" = true ]; then
        print_success "Verificación de dependencias completada"
        exit 0
    fi

    # Compilar
    build_program

    # Probar
    if ! test_program; then
        print_error "Las pruebas fallaron"
        exit 1
    fi

    if [ "$test_only" = true ]; then
        print_success "Compilación y pruebas completadas"
        print_info "El binario está en: builddir/wakefull"
        exit 0
    fi

    # Instalar
    if [ "$install_type" = "system" ]; then
        if install_system; then
            print_success "Instalación del sistema completada"
        else
            exit 1
        fi
    else
        if install_user; then
            print_success "Instalación de usuario completada"
        else
            exit 1
        fi
    fi

    echo ""
    print_success "¡Instalación completa!"
    echo ""
    print_info "Comandos disponibles:"
    print_info "  wakefull --start    # Iniciar bloqueo de protector de pantalla"
    print_info "  wakefull --stop     # Detener bloqueo"
    print_info "  wakefull --status   # Ver estado actual"
    print_info "  wakefull --test     # Probar métodos disponibles"
    print_info "  wakefull --help     # Mostrar ayuda"
    print_info "  wakefull --version  # Ver versión"
    echo ""

    # Instrucciones específicas para XFCE4
    if is_xfce4; then
        print_info "Instrucciones específicas para XFCE4:"
        print_info "• Wakefull detectará y configurará XFCE4 automáticamente"
        print_info "• Incluye manejo nativo de xfce4-power-manager y xfce4-screensaver"
        print_info "• Las configuraciones se restauran automáticamente al parar"
        echo ""
        print_success "XFCE4 totalmente soportado con método específico integrado"
        echo ""
    fi
}

# Ejecutar función principal
main "$@"
