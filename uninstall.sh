#!/bin/bash

set -e

PROGRAM_NAME="wakefull"
PREFIX="/usr/local"
LOCK_FILE="/tmp/wakefull.lock"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

print_question() {
    echo -e "${BLUE}[QUESTION]${NC} $1"
}

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --prefix PREFIX    Installation prefix to remove from (default: /usr/local)"
    echo "  --force           Skip confirmation prompts"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Uninstall from /usr/local"
    echo "  $0 --prefix /usr             # Uninstall from /usr"
    echo "  $0 --force                   # Uninstall without confirmation"
    echo ""
}

ask_confirmation() {
    local message="$1"

    if [ "$FORCE" = "true" ]; then
        return 0
    fi

    print_question "$message (y/N): "
    read -r response
    case "$response" in
        [yY][eE][sS]|[yY])
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

stop_running_daemon() {
    # Check if wakefull is installed and running
    local wakefull_path="$PREFIX/bin/$PROGRAM_NAME"

    if [ -f "$wakefull_path" ]; then
        print_status "Checking if $PROGRAM_NAME is currently running..."

        if [ -f "$LOCK_FILE" ]; then
            print_warning "$PROGRAM_NAME appears to be running. Attempting to stop it..."

            if "$wakefull_path" --stop; then
                print_status "Successfully stopped $PROGRAM_NAME daemon"
            else
                print_warning "Failed to stop $PROGRAM_NAME cleanly"

                # Try to clean up lock file manually
                if [ -f "$LOCK_FILE" ]; then
                    print_status "Cleaning up lock file manually..."
                    rm -f "$LOCK_FILE" || print_warning "Could not remove lock file"
                fi
            fi
        else
            print_status "$PROGRAM_NAME is not currently running"
        fi
    fi
}

remove_files() {
    local wakefull_path="$PREFIX/bin/$PROGRAM_NAME"
    local files_removed=0

    print_status "Removing installed files..."

    # Remove the main binary
    if [ -f "$wakefull_path" ]; then
        if [ "$EUID" -eq 0 ]; then
            rm -f "$wakefull_path"
        else
            sudo rm -f "$wakefull_path"
        fi

        if [ ! -f "$wakefull_path" ]; then
            print_status "Removed: $wakefull_path"
            files_removed=$((files_removed + 1))
        else
            print_error "Failed to remove: $wakefull_path"
        fi
    else
        print_warning "Binary not found at: $wakefull_path"
    fi

    # Clean up any remaining lock files
    if [ -f "$LOCK_FILE" ]; then
        rm -f "$LOCK_FILE" || print_warning "Could not remove lock file"
        print_status "Cleaned up lock file: $LOCK_FILE"
        files_removed=$((files_removed + 1))
    fi

    return $files_removed
}

main() {
    local FORCE="false"

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --prefix)
                PREFIX="$2"
                shift 2
                ;;
            --force)
                FORCE="true"
                shift
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

    echo "===================================="
    echo "   Wakefull Uninstallation Script   "
    echo "===================================="
    echo ""
    print_status "Uninstalling from prefix: $PREFIX"
    echo ""

    # Check if wakefull is actually installed
    if [ ! -f "$PREFIX/bin/$PROGRAM_NAME" ]; then
        print_warning "$PROGRAM_NAME does not appear to be installed at $PREFIX/bin/"

        if ! ask_confirmation "Continue with cleanup anyway?"; then
            print_status "Uninstallation cancelled"
            exit 0
        fi
    fi

    # Confirm uninstallation
    if ! ask_confirmation "Are you sure you want to uninstall $PROGRAM_NAME?"; then
        print_status "Uninstallation cancelled"
        exit 0
    fi

    echo ""

    # Stop any running daemon
    stop_running_daemon
    echo ""

    # Remove files
    if remove_files; then
        local files_count=$?
        echo ""
        print_status "Uninstallation completed!"

        if [ $files_count -gt 0 ]; then
            print_status "Removed $files_count file(s)"
        fi

        echo ""
        print_status "Thank you for using Wakefull!"
    else
        echo ""
        print_error "Uninstallation completed with errors"
        print_error "Some files may not have been removed properly"
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
