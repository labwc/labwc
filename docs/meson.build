scdoc = find_program('scdoc', required: get_option('man-pages'))

if scdoc.found()
  sh = find_program('sh', native: true)
  sections = ['.1', '-config.5', '-theme.5', '-actions.5', '-menu.5']
  foreach s : sections
    markdown = 'labwc' + s + '.scd'
    manpage = 'labwc' + s
    custom_target(
      manpage,
      input: markdown,
      command: [sh, '-c', 'scdoc < @INPUT@'],
      output: manpage,
      capture: true,
      install: true,
      install_dir: join_paths(get_option('mandir'), 'man' + s.split('.')[-1])
    )
  endforeach
endif
