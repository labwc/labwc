wl_protocol_dir = wayland_protos.get_variable(pkgconfig: 'pkgdatadir')
wayland_scanner = find_program('wayland-scanner')

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_server = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.h',
	arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
)

server_protocols = [
	[wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
	['wlr-layer-shell-unstable-v1.xml'],
	['wlr-input-inhibitor-unstable-v1.xml'],
]

server_protos_src = []
server_protos_headers = []

foreach p : server_protocols
	xml = join_paths(p)
	server_protos_src += wayland_scanner_code.process(xml)
	server_protos_headers += wayland_scanner_server.process(xml)
endforeach

lib_server_protos = static_library(
	'server_protos',
	server_protos_src + server_protos_headers,
	dependencies: [wayland_server]
)

server_protos = declare_dependency(
	link_with: lib_server_protos,
	sources: server_protos_headers,
)
