labwc_sources = files(
  'action.c',
  'cursor.c',
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
  'view.c',
  'xdg.c',
)

if have_xwayland
  labwc_sources += files(
    'xwayland.c',
    'xwayland-unmanaged.c',
  )
endif


subdir('common')
subdir('config')
subdir('theme')
subdir('xbm')
subdir('menu')
