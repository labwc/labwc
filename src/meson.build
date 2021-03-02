labwc_sources = files(
  'action.c',
  'cursor.c',
  'damage.c',
  'deco.c',
  'desktop.c',
  'interactive.c',
  'keyboard.c',
  'layers.c',
  'main.c',
  'osd.c',
  'output.c',
  'seat.c',
  'server.c',
  'subsurface.c',
  'theme.c',
  'view.c',
  'view-child.c',
  'xdg.c',
  'xdg-deco.c',
  'xdg-popup.c',
)

if have_xwayland
  labwc_sources += files(
    'xwayland.c',
    'xwayland-unmanaged.c',
  )
endif


subdir('common')
subdir('config')
subdir('xbm')
subdir('menu')
