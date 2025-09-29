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
    print_status "Checking dependencies..."

    # Check for meson
    if ! command -v meson &> /dev/null; then
        print_error "meson is required but not installed."
        print_error "Install with: sudo apt install meson (Ubuntu/Debian) or sudo pacman -S meson (Arch)"
        exit 1
    fi

    # Check for ninja
    if ! command -v ninja &> /dev/null; then
        print_error "ninja is required but not installed."
        print_error "Install with: sudo apt install ninja-build (Ubuntu/Debian) or sudo pacman -S ninja (Arch)"
        exit 1
    fi

    # Check for X11 development libraries
    if ! pkg-config --exists x11; then
        print_error "X11 development libraries are required but not found."
        print_error "Install with: sudo apt install libx11-dev (Ubuntu/Debian) or sudo pacman -S libx11 (Arch)"
        exit 1
    fi

    # Check for xdg-screensaver
    if ! command -v xdg-screensaver &> /dev/null; then
        print_warning "xdg-screensaver not found. This is required for wakefull to function properly."
        print_warning "Install with: sudo apt install xdg-utils (Ubuntu/Debian) or sudo pacman -S xdg-utils (Arch)"
    fi

    print_status "All dependencies satisfied!"
}

build_program() {
    print_status "Setting up build directory..."

    # Clean previous build if it exists
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi

    # Setup meson build
    print_status "Configuring build with meson..."
    meson setup "$BUILD_DIR" --prefix="$PREFIX"

    # Build the program
    print_status "Building $PROGRAM_NAME..."
    ninja -C "$BUILD_DIR"

    print_status "Build completed successfully!"
}

install_program() {
    print_status "Installing $PROGRAM_NAME to $PREFIX/bin..."

    # Install using ninja
    if [ "$EUID" -eq 0 ]; then
        ninja -C "$BUILD_DIR" install
    else
        sudo ninja -C "$BUILD_DIR" install
    fi

    print_status "$PROGRAM_NAME installed successfully!"
    print_status "You can now run: $PROGRAM_NAME --start"
}

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --prefix PREFIX    Install prefix (default: /usr/local)"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Install to /usr/local"
    echo "  $0 --prefix /usr             # Install to /usr"
    echo ""
}

cleanup() {
    if [ -d "$BUILD_DIR" ]; then
        print_status "Cleaning up build directory..."
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
                print_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done

    echo "================================"
    echo "  Wakefull Installation Script  "
    echo "================================"
    echo ""
    print_status "Installing to prefix: $PREFIX"
    echo ""

    # Set trap for cleanup on exit
    trap cleanup EXIT

    # Check if we're in the right directory
    if [ ! -f "wakefull.c" ] || [ ! -f "meson.build" ]; then
        print_error "Please run this script from the wakefull source directory"
        exit 1
    fi

    # Run installation steps
    check_dependencies
    build_program
    install_program

    echo ""
    echo "================================"
    print_status "Installation completed successfully!"
    echo "================================"
    echo ""
    print_status "Wakefull has been installed to: $PREFIX/bin/$PROGRAM_NAME"
    echo ""
    print_status "Quick Start Guide:"
    echo "  $PROGRAM_NAME --start     # Start daemon to prevent desktop idleness"
    echo "  $PROGRAM_NAME --status    # Check if wakefull is running"
    echo "  $PROGRAM_NAME --stop      # Stop preventing desktop idleness"
    echo "  $PROGRAM_NAME --help      # Show detailed help"
    echo ""
    print_status "Example Usage:"
    echo "  # Before watching a movie or giving a presentation:"
    echo "  $PROGRAM_NAME --start"
    echo ""
    echo "  # When finished:"
    echo "  $PROGRAM_NAME --stop"
    echo ""
    print_status "Notes:"
    echo "  - Wakefull runs as a background daemon when started"
    echo "  - Your screen will stay awake until you stop wakefull"
    echo "  - Use Ctrl+C if running in foreground, or --stop to cleanly exit"
    echo "  - Lock file: /tmp/wakefull.lock (contains daemon PID and window ID)"
    echo ""
}

# Run main function with all arguments
main "$@"
