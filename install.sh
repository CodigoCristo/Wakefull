#!/bin/bash

set -e

PROGRAM_NAME="wakefull"
BUILD_DIR="build"
PREFIX="/usr/local"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    print_status "Verificando dependencias..."

    # Check for meson
    if ! command -v meson &> /dev/null; then
        print_error "meson es requerido pero no está instalado."
        print_error "Instala con: sudo apt install meson (Ubuntu/Debian) o sudo pacman -S meson (Arch)"
        exit 1
    fi

    # Check for ninja
    if ! command -v ninja &> /dev/null; then
        print_error "ninja es requerido pero no está instalado."
        print_error "Instala con: sudo apt install ninja-build (Ubuntu/Debian) o sudo pacman -S ninja (Arch)"
        exit 1
    fi

    # Check for X11 development libraries
    if ! pkg-config --exists x11; then
        print_error "Las librerías de desarrollo X11 son requeridas pero no se encontraron."
        print_error "Instala con: sudo apt install libx11-dev (Ubuntu/Debian) o sudo pacman -S libx11 (Arch)"
        exit 1
    fi

    # Check for xdg-screensaver
    if ! command -v xdg-screensaver &> /dev/null; then
        print_warning "xdg-screensaver no encontrado. Esto es requerido para que wakefull funcione correctamente."
        print_warning "Instala con: sudo apt install xdg-utils (Ubuntu/Debian) o sudo pacman -S xdg-utils (Arch)"
    fi

    print_status "¡Todas las dependencias están satisfechas!"
}

build_program() {
    print_status "Configurando directorio de construcción..."

    # Clean previous build if it exists
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi

    # Setup meson build
    print_status "Configurando construcción con meson..."
    meson setup "$BUILD_DIR" --prefix="$PREFIX"

    # Build the program
    print_status "Construyendo $PROGRAM_NAME..."
    ninja -C "$BUILD_DIR"

    print_status "¡Construcción completada exitosamente!"
}

install_program() {
    print_status "Instalando $PROGRAM_NAME en $PREFIX/bin..."

    # Install using ninja
    if [ "$EUID" -eq 0 ]; then
        ninja -C "$BUILD_DIR" install
    else
        sudo ninja -C "$BUILD_DIR" install
    fi

    print_status "¡$PROGRAM_NAME instalado exitosamente!"
    print_status "Ahora puedes ejecutar: $PROGRAM_NAME --start"
}

print_usage() {
    echo "Uso: $0 [OPCIONES]"
    echo ""
    echo "Opciones:"
    echo "  --prefix PREFIX    Prefijo de instalación (por defecto: /usr/local)"
    echo "  --help            Mostrar este mensaje de ayuda"
    echo ""
    echo "Ejemplos:"
    echo "  $0                           # Instalar en /usr/local"
    echo "  $0 --prefix /usr             # Instalar en /usr"
    echo ""
}

cleanup() {
    if [ -d "$BUILD_DIR" ]; then
        print_status "Limpiando directorio de construcción..."
        rm -rf "$BUILD_DIR"
    fi
}

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --prefix)
                PREFIX="$2"
                shift 2
                ;;
            --help)
                print_usage
                exit 0
                ;;
            *)
                print_error "Opción desconocida: $1"
                print_usage
                exit 1
                ;;
        esac
    done

    echo "================================"
    echo "   Script de Instalación Wakefull   "
    echo "================================"
    echo ""
    print_status "Instalando en prefijo: $PREFIX"
    echo ""

    # Set trap for cleanup on exit
    trap cleanup EXIT

    # Check if we're in the right directory
    if [ ! -f "wakefull.c" ] || [ ! -f "meson.build" ]; then
        print_error "Por favor ejecuta este script desde el directorio fuente de wakefull"
        exit 1
    fi

    # Run installation steps
    check_dependencies
    build_program
    install_program

    echo ""
    echo "================================"
    print_status "¡Instalación completada exitosamente!"
    echo "================================"
    echo ""
    print_status "Wakefull ha sido instalado en: $PREFIX/bin/$PROGRAM_NAME"
    echo ""
    print_status "Guía de Inicio Rápido:"
    echo "  $PROGRAM_NAME --start     # Iniciar daemon para prevenir inactividad del escritorio"
    echo "  $PROGRAM_NAME --status    # Verificar si wakefull está funcionando"
    echo "  $PROGRAM_NAME --stop      # Detener prevención de inactividad del escritorio"
    echo "  $PROGRAM_NAME --help      # Mostrar ayuda detallada"
    echo ""
    print_status "Ejemplo de Uso:"
    echo "  # Antes de ver una película o dar una presentación:"
    echo "  $PROGRAM_NAME --start"
    echo ""
    echo "  # Cuando termines:"
    echo "  $PROGRAM_NAME --stop"
    echo ""
    print_status "Notas:"
    echo "  - Wakefull se ejecuta como un daemon en segundo plano al iniciarse"
    echo "  - Tu pantalla se mantendrá despierta hasta que detengas wakefull"
    echo "  - Usa Ctrl+C si se ejecuta en primer plano, o --stop para salir limpiamente"
    echo "  - Archivo de bloqueo: /tmp/wakefull.lock (contiene PID del daemon e ID de ventana)"
    echo ""
}

# Run main function with all arguments
main "$@"
