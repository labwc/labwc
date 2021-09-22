scdoc = find_program('scdoc', required: get_option('man-pages'))

if scdoc.found()
  sections = [
    '.1',
    '-actions.5',
    '-config.5',
    '-environment.5',
    '-menu.5',
    '-theme.5',
  ]
  foreach s : sections
    markdown = 'labwc' + s + '.scd'
    manpage = 'labwc' + s
    custom_target(
      manpage,
      input: markdown,
      output: manpage,
      command: scdoc,
      feed: true,
      capture: true,
      install: true,
      install_dir: join_paths(get_option('mandir'), 'man' + s.split('.')[-1])
    )
  endforeach
endif
