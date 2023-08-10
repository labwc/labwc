labwc_sources = files(
  'action.c',
  'buffer.c',
  'cursor.c',
  'debug.c',
  'desktop.c',
  'dnd.c',
  'foreign.c',
  'idle.c',
  'interactive.c',
  'keyboard.c',
  'key-state.c',
  'layers.c',
  'main.c',
  'node.c',
  'osd.c',
  'output.c',
  'button-png.c',
  'regions.c',
  'resistance.c',
  'seat.c',
  'server.c',
  'session-lock.c',
  'touch.c',
  'theme.c',
  'view.c',
  'view-impl-common.c',
  'window-rules.c',
  'workspaces.c',
  'xdg.c',
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
subdir('decorations')
subdir('button')
subdir('menu')
subdir('ssd')
