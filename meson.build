project('wakefull', 'c',
  version: '2.2.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'warning_level=2',
    'c_std=c99',
    'optimization=2'
  ]
)

# Información del proyecto
project_description = 'Bloqueador completo de protector de pantalla, suspensión, hibernación y cierre de tapa para XFCE4'

# Verificar herramientas opcionales disponibles en el sistema
systemd_inhibit = find_program('systemd-inhibit', required: false)
xdg_screensaver = find_program('xdg-screensaver', required: false)
xset = find_program('xset', required: false)
xfconf_query = find_program('xfconf-query', required: false)

# Compilar el ejecutable principal (sin dependencias externas)
executable('wakefull',
  'wakefull.c',
  install: true,
  install_dir: get_option('bindir')
)

# Mostrar información útil
message('=== ' + meson.project_name() + ' v' + meson.project_version() + ' ===')
message(project_description)
message('')
message('Herramientas detectadas en el sistema:')

if xset.found()
  message('✓ xset: ' + xset.full_path())
else
  message('✗ xset: No encontrado (recomendado: sudo apt install x11-xserver-utils)')
endif

if xfconf_query.found()
  message('✓ xfconf-query: ' + xfconf_query.full_path())
else
  message('✗ xfconf-query: No encontrado (recomendado: sudo apt install xfconf)')
endif

if xdg_screensaver.found()
  message('✓ xdg-screensaver: ' + xdg_screensaver.full_path())
else
  message('✗ xdg-screensaver: No encontrado (opcional)')
endif

if systemd_inhibit.found()
  message('✓ systemd-inhibit: ' + systemd_inhibit.full_path())
else
  message('✗ systemd-inhibit: No encontrado (opcional)')
endif

message('')
message('El programa funcionará con las herramientas disponibles.')
message('Para funcionalidad completa se recomienda: xset, xfconf-query, systemd, dbus-send')
message('Previene: protector de pantalla, suspensión, hibernación y cierre de tapa.')
