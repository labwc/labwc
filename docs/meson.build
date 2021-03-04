pandoc = find_program('pandoc', required: false)

sections = ['.1', '-config.5', '-theme.5', '-actions.5']

if pandoc.found()
  foreach s : sections
    markdown = files('labwc' + s + '.md')
    manpage = 'labwc' + s
    custom_target(
      manpage,
      command: [pandoc,'-s','-t','man',markdown],
      output: manpage,
      capture: true,
      install: true,
      install_dir: join_paths(get_option('mandir'), 'man' + s.split('.')[-1])
    )
  endforeach
endif
