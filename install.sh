#!/bin/bash

# Script de instalación simple para wakefull
# Copyright (C) 2024
# Versión simplificada para XFCE4

set -e  # Salir en caso de error

PROGRAM_NAME="wakefull"
VERSION="2.2.0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="/usr/local"

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[ÉXITO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[AVISO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_header() {
    echo "=================================="
    echo "  Instalador de $PROGRAM_NAME $VERSION"
    echo "=================================="
    echo
}

check_permissions() {
    if [[ $EUID -eq 0 ]]; then
        PREFIX="/usr/local"
        print_info "Instalando como root en $PREFIX"
    else
        PREFIX="$HOME/.local"
        print_info "Instalando como usuario en $PREFIX"
        mkdir -p "$PREFIX/bin"
    fi
}

check_dependencies() {
    print_info "Verificando dependencias..."

    if ! command -v gcc >/dev/null 2>&1; then
        print_error "gcc no encontrado"
        print_error "Instala: sudo apt install gcc"
        exit 1
    fi

    print_success "gcc encontrado"
}

show_system_info() {
    print_info "Información del sistema:"
    echo "  Desktop: ${XDG_CURRENT_DESKTOP:-No detectado}"
    echo "  Session: ${DESKTOP_SESSION:-No detectado}"
    echo "  Display: ${DISPLAY:-No detectado}"
    echo

    print_info "Herramientas disponibles:"
    command -v xset >/dev/null 2>&1 && echo "  ✓ xset" || echo "  ✗ xset (recomendado: sudo apt install x11-xserver-utils)"
    command -v xfconf-query >/dev/null 2>&1 && echo "  ✓ xfconf-query" || echo "  ✗ xfconf-query (recomendado: sudo apt install xfconf)"
    command -v xdg-screensaver >/dev/null 2>&1 && echo "  ✓ xdg-screensaver" || echo "  ✗ xdg-screensaver (opcional)"
}

cleanup_previous() {
    print_info "Limpiando instalación anterior..."

    # Detener instancia existente si hay una
    if [ -f "/tmp/wakefull.pid" ]; then
        if [ -x "$PREFIX/bin/$PROGRAM_NAME" ]; then
            "$PREFIX/bin/$PROGRAM_NAME" --stop >/dev/null 2>&1 || true
        fi
    fi

    # Limpiar archivos temporales
    rm -f /tmp/wakefull.pid /tmp/wakefull.log /tmp/.wakefull_keepalive
}

compile_program() {
    print_info "Compilando $PROGRAM_NAME..."

    cd "$SCRIPT_DIR"

    if [ ! -f "wakefull.c" ]; then
        print_error "archivo wakefull.c no encontrado"
        exit 1
    fi

    # Compilar con configuración optimizada
    gcc -Wall -Wextra -std=c99 -O2 -DNDEBUG -o "$PROGRAM_NAME" wakefull.c

    if [ ! -f "$PROGRAM_NAME" ]; then
        print_error "Compilación falló"
        exit 1
    fi

    print_success "Compilación exitosa"
}

install_program() {
    print_info "Instalando $PROGRAM_NAME en $PREFIX/bin..."

    mkdir -p "$PREFIX/bin"
    cp "$SCRIPT_DIR/$PROGRAM_NAME" "$PREFIX/bin/$PROGRAM_NAME"
    chmod +x "$PREFIX/bin/$PROGRAM_NAME"

    print_success "Instalación completada"
}

verify_installation() {
    if [ -x "$PREFIX/bin/$PROGRAM_NAME" ]; then
        print_success "Instalación verificada"
        "$PREFIX/bin/$PROGRAM_NAME" --version
        return 0
    else
        print_error "Verificación de instalación falló"
        return 1
    fi
}

show_usage_instructions() {
    echo
    print_info "Instrucciones de uso:"
    echo
    echo "Para usar $PROGRAM_NAME:"
    echo "  $PREFIX/bin/$PROGRAM_NAME --start    # Iniciar daemon"
    echo "  $PREFIX/bin/$PROGRAM_NAME --stop     # Detener daemon"
    echo "  $PREFIX/bin/$PROGRAM_NAME --status   # Ver estado"
    echo "  $PREFIX/bin/$PROGRAM_NAME --help     # Ver ayuda completa"
    echo

    if [[ $EUID -ne 0 ]] && [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
        print_warning "Para usar $PROGRAM_NAME desde cualquier lugar, agrega esto a tu ~/.bashrc:"
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
        echo "Luego ejecuta: source ~/.bashrc"
        echo
    fi

    print_info "Este programa previene que XFCE4 active el protector de pantalla"
    print_info "o ponga el sistema en suspensión."
}

# Función principal
main() {
    show_header
    check_permissions
    show_system_info
    echo
    check_dependencies
    cleanup_previous
    compile_program
    install_program

    if verify_installation; then
        show_usage_instructions
        echo
        print_success "¡Instalación completada exitosamente!"

        # Ofrecer prueba rápida
        echo
        read -p "¿Quieres iniciar wakefull ahora? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            "$PREFIX/bin/$PROGRAM_NAME" --start
        fi
    else
        print_error "Instalación falló"
        exit 1
    fi
}

# Manejar Ctrl+C
trap 'echo -e "\n${YELLOW}Instalación cancelada${NC}"; exit 1' INT

# Ejecutar
main "$@"
