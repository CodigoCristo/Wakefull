#!/bin/bash
#
# clean.sh - Script de limpieza para wakefull
#

set -e

# Colores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_info "Limpiando proyecto wakefull..."

# Limpiar directorio de build de Meson
if [ -d "builddir" ]; then
    rm -rf builddir
    print_success "Directorio builddir eliminado"
fi

# Limpiar archivos temporales del programa
rm -f /tmp/wakefull.* 2>/dev/null && print_success "Archivos temporales eliminados" || true

# Limpiar archivos de respaldo del editor
find . -name "*~" -delete 2>/dev/null && print_success "Archivos de respaldo eliminados" || true
find . -name "*.swp" -delete 2>/dev/null || true
find . -name "*.swo" -delete 2>/dev/null || true

# Limpiar archivos de core dump
find . -name "core" -delete 2>/dev/null || true

print_success "Limpieza completada"

echo ""
print_info "Para recompilar:"
print_info "  meson setup builddir"
print_info "  meson compile -C builddir"
print_info "O usar el instalador:"
print_info "  ./install.sh"
