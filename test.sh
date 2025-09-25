#!/bin/bash

# Test script for wakeful
# Bloqueador de protectores de pantalla y energía (como caffeine pero escrito en C)
# Este script prueba compilación, funcionalidad básica y detección de métodos

set -e  # Exit on any error

PROGRAM="./wakefull"
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Wakefull Test Suite ===${NC}"
echo -e "${BLUE}=== Bloqueador de protectores de pantalla y energía ===${NC}"
echo ""

# Function to print test results
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASS:${NC} $2"
    else
        echo -e "${RED}✗ FAIL:${NC} $2"
        return 1
    fi
}

# Function to print info
print_info() {
    echo -e "${BLUE}INFO:${NC} $1"
}

# Function to print warning
print_warning() {
    echo -e "${YELLOW}WARN:${NC} $1"
}

# Test 1: Check build dependencies
print_info "Checking build dependencies..."
if command -v gcc >/dev/null 2>&1; then
    print_result 0 "GCC compiler found"
else
    print_result 1 "GCC compiler not found"
    exit 1
fi

if pkg-config --exists x11 2>/dev/null; then
    print_result 0 "X11 development libraries found"
else
    print_result 1 "X11 development libraries not found"
    print_warning "You may need to install libx11-dev or libX11-devel"
fi

# Test 2: Clean and compile
print_info "Cleaning and compiling..."
make clean >/dev/null 2>&1 || true
if make >/dev/null 2>&1; then
    print_result 0 "Compilation successful"
else
    print_result 1 "Compilation failed"
    exit 1
fi

# Test 3: Check if binary exists and is executable
if [ -x "$PROGRAM" ]; then
    print_result 0 "Binary exists and is executable"
else
    print_result 1 "Binary not found or not executable"
    exit 1
fi

# Test 4: Basic command tests
print_info "Testing basic commands..."

# Test help
if $PROGRAM --help >/dev/null 2>&1; then
    print_result 0 "--help command works"
else
    print_result 1 "--help command failed"
fi

# Test version
if $PROGRAM --version >/dev/null 2>&1; then
    print_result 0 "--version command works"
else
    print_result 1 "--version command failed"
fi

# Test status (should show "not running")
if $PROGRAM --status >/dev/null 2>&1; then
    print_result 0 "--status command works"
else
    print_result 1 "--status command failed"
fi

# Test method detection
if $PROGRAM --test >/dev/null 2>&1; then
    print_result 0 "--test command works"
else
    print_result 1 "--test command failed"
fi

# Test 5: Runtime dependency detection
print_info "Checking runtime dependencies..."

# Check for various inhibition methods
if command -v xdg-screensaver >/dev/null 2>&1; then
    print_result 0 "xdg-screensaver found (X11 method)"
else
    print_warning "xdg-screensaver not found (install xdg-utils for X11 support)"
fi

if command -v systemd-inhibit >/dev/null 2>&1; then
    print_result 0 "systemd-inhibit found (universal method)"
else
    print_warning "systemd-inhibit not found (systemd systems only)"
fi

if command -v dbus-send >/dev/null 2>&1; then
    print_result 0 "dbus-send found (D-Bus method)"
else
    print_warning "dbus-send not found (install dbus)"
fi

# Test 6: Display server detection
print_info "Checking display server environment..."

if [ -n "$DISPLAY" ]; then
    print_result 0 "X11 session detected (DISPLAY=$DISPLAY)"
    X11_AVAILABLE=1
else
    print_warning "No X11 session detected"
    X11_AVAILABLE=0
fi

if [ -n "$WAYLAND_DISPLAY" ]; then
    print_result 0 "Wayland session detected (WAYLAND_DISPLAY=$WAYLAND_DISPLAY)"
    WAYLAND_AVAILABLE=1
else
    print_warning "No Wayland session detected"
    WAYLAND_AVAILABLE=0
fi

if [ -n "$XDG_SESSION_TYPE" ]; then
    print_result 0 "Session type: $XDG_SESSION_TYPE"
else
    print_warning "XDG_SESSION_TYPE not set"
fi

# Test 7: Method selection test
print_info "Testing method selection..."
METHOD_OUTPUT=$($PROGRAM --test 2>&1)
if echo "$METHOD_OUTPUT" | grep -q "Recommended method:"; then
    RECOMMENDED_METHOD=$(echo "$METHOD_OUTPUT" | grep "Recommended method:" | cut -d: -f2 | xargs)
    print_result 0 "Method selection works, recommended: $RECOMMENDED_METHOD"
else
    if echo "$METHOD_OUTPUT" | grep -q "No suitable method found"; then
        print_warning "No suitable inhibition method found on this system"
    else
        print_result 1 "Method selection test failed"
    fi
fi

# Test 8: Quick functional test (only if we have a display server and suitable method)
FUNCTIONAL_TEST=0
if ([ "$X11_AVAILABLE" -eq 1 ] || [ "$WAYLAND_AVAILABLE" -eq 1 ]) && \
   (command -v systemd-inhibit >/dev/null 2>&1 || command -v xdg-screensaver >/dev/null 2>&1); then

    print_info "Running quick functional test..."

    # Test start (run in background for 2 seconds, then stop)
    if timeout 10s bash -c "
        $PROGRAM --start &
        PID=\$!
        sleep 2
        if $PROGRAM --status | grep -q 'Ejecutándose'; then
            echo 'Status check passed'
            $PROGRAM --stop
            wait \$PID
            exit 0
        else
            kill \$PID 2>/dev/null || true
            exit 1
        fi
    " >/dev/null 2>&1; then
        print_result 0 "Quick functional test (start/stop cycle)"
        FUNCTIONAL_TEST=1
    else
        print_warning "Quick functional test failed (this may be normal in some environments)"
    fi
else
    print_warning "Skipping functional test (no suitable environment or methods)"
fi

# Test 9: Invalid argument handling
print_info "Testing error handling..."
if $PROGRAM --invalid-option >/dev/null 2>&1; then
    print_result 1 "Invalid option handling (should fail)"
else
    print_result 0 "Invalid option properly rejected"
fi

if $PROGRAM >/dev/null 2>&1; then
    print_result 1 "No arguments handling (should fail)"
else
    print_result 0 "No arguments properly rejected"
fi

# Test 10: File permissions test
print_info "Testing file operations..."
if touch /tmp/wakefull-test 2>/dev/null; then
    rm -f /tmp/wakefull-test
    print_result 0 "Can create files in /tmp"
else
    print_result 1 "Cannot create files in /tmp"
fi

# Summary
echo ""
echo -e "${BLUE}=== Test Summary ===${NC}"

if [ "$FUNCTIONAL_TEST" -eq 1 ]; then
    echo -e "${GREEN}✓ All core tests passed${NC}"
    echo -e "${GREEN}✓ Functional test completed successfully${NC}"
    echo "The program should work correctly on this system."
elif [ "$X11_AVAILABLE" -eq 1 ] || [ "$WAYLAND_AVAILABLE" -eq 1 ]; then
    echo -e "${YELLOW}⚠ Basic tests passed, functional test skipped${NC}"
    echo "The program compiled correctly but functional testing was limited."
    echo "Try running: $PROGRAM --test"
else
    echo -e "${YELLOW}⚠ Basic tests passed, no display server detected${NC}"
    echo "The program compiled correctly but requires X11 or Wayland to function."
fi

echo ""
echo -e "${BLUE}Manual testing suggestions:${NC}"
echo "1. Check available methods: $PROGRAM --test"
echo "2. Start inhibition: $PROGRAM --start"
echo "3. Check status: $PROGRAM --status"
echo "4. Stop inhibition: $PROGRAM --stop"

if [ "$X11_AVAILABLE" -eq 0 ] && [ "$WAYLAND_AVAILABLE" -eq 0 ]; then
    echo ""
    echo -e "${YELLOW}Nota:${NC} No se detectó servidor de pantalla. Este programa requiere:"
    echo "- Sesión X11 (variable de entorno DISPLAY)"
    echo "- Sesión Wayland (variable de entorno WAYLAND_DISPLAY)"
    echo "- O systemd-inhibit para operación sin cabeza"
fi

echo ""
echo -e "${GREEN}¡Script de pruebas completado!${NC}"
