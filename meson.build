project('wakefull', 'c',
  version: '2.1.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'warning_level=2',
    'c_std=c99',
    'optimization=2'
  ]
)

# Dependencias
x11_dep = dependency('x11')

# Información del proyecto
project_description = 'Bloqueador simple de protector de pantalla'

# Verificar dependencias opcionales en tiempo de build
systemd_inhibit = find_program('systemd-inhibit', required: false)
xdg_screensaver = find_program('xdg-screensaver', required: false)
dbus_send = find_program('dbus-send', required: false)

# Advertir si no hay métodos disponibles
if not systemd_inhibit.found() and not xdg_screensaver.found() and not dbus_send.found()
  warning('No se encontraron herramientas de inhibición. El programa compilará pero puede no funcionar.')
  message('Instala al menos uno de estos paquetes:')
  message('  - systemd (para systemd-inhibit)')
  message('  - xdg-utils (para xdg-screensaver)')
  message('  - dbus (para dbus-send)')
endif

# Compilar el ejecutable principal
executable('wakefull',
  'wakefull.c',
  dependencies: [x11_dep],
  install: true,
  install_dir: get_option('bindir')
)

# Información de instalación
message('Proyecto: ' + meson.project_name() + ' v' + meson.project_version())
message('Descripción: ' + project_description)

if systemd_inhibit.found()
  message('✓ systemd-inhibit encontrado: ' + systemd_inhibit.full_path())
endif

if xdg_screensaver.found()
  message('✓ xdg-screensaver encontrado: ' + xdg_screensaver.full_path())
endif

if dbus_send.found()
  message('✓ dbus-send encontrado: ' + dbus_send.full_path())
endif
