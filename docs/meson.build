scdoc = find_program('scdoc', required: get_option('man-pages'))

if scdoc.found()
  manpages = [
    'labwc.1',
    'labwc-actions.5',
    'labwc-config.5',
    'labwc-menu.5',
    'labwc-theme.5',
    'labnag.1',
  ]
  foreach manpage : manpages
    markdown = manpage + '.scd'
    custom_target(
      manpage,
      input: markdown,
      output: manpage,
      command: scdoc,
      feed: true,
      capture: true,
      install: true,
      install_dir: join_paths(get_option('mandir'), 'man' + manpage.split('.')[-1])
    )
  endforeach
endif

install_data(
  [
    'autostart',
    'environment',
    'menu.xml',
    'README',
    'shutdown',
    'themerc',
    'rc.xml',
    'rc.xml.all'
  ],
  install_dir: get_option('datadir') / 'doc' / meson.project_name()
)
