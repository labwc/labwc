#!/usr/bin/env python3

import os
import socket
import struct

# object ids
WL_DISPLAY = 1
WL_REGISTRY = 2
WL_SYNC_DONE = 3

# opcodes requests
WL_DISPLAY_SYNC = 0
WL_DISPLAY_GET_REGISTRY = 1

# opcodes events
WL_REGISTRY_GLOBAL = 0

class ArgString:
	def parse(data):
		size = struct.unpack('=I', data[:4])[0]
		data = data[4:4 + size - 1]
		padding = (4 - (size % 4)) % 4
		return 4 + size + padding, data.decode()

class ArgRegistryGlobal:
	def parse(data):
		global_id = struct.unpack('=I', data[:4])[0]
		data = data[4:]
		consumed, interface = ArgString.parse(data)
		data = data[consumed:]
		version = struct.unpack('=I', data)[0]
		return global_id, interface, version

class Wayland:
	def __init__(self, wl_socket, log=False):
		if log:
			print()
			print(f'  Connecting to {wl_socket}')
		self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
		self.socket.connect(wl_socket)
		try:
			creds = self.socket.getsockopt(
				socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize('3i')
			)
			pid, uid, gid = struct.unpack('3i', creds)
			with open(f'/proc/{pid}/comm', 'r') as f:
				self.name = f.read().strip()
		except:
			self.name = 'Unknown'
		if log:
			print(f"  Connected to {self.name}\n")

	def wire(self, obj_id, opcode, data=b''):
		size = 8 + len(data)
		sizeop = size << 16 | opcode
		self.socket.send(struct.pack('=II', obj_id, sizeop) + data)

	def wire_arg_uint32(self, to_obj_id, opcode, arg):
		self.wire(to_obj_id, opcode, struct.pack('=I', arg))

	def parse_msg(self, obj_id, opcode, arg_data, interfaces):
		if obj_id == WL_REGISTRY and opcode == WL_REGISTRY_GLOBAL:
			_, interface, version = ArgRegistryGlobal.parse(arg_data)
			interfaces[interface] = version
			return True
		return False

	def get_interfaces(self):
		self.wire_arg_uint32(WL_DISPLAY, WL_DISPLAY_GET_REGISTRY, WL_REGISTRY)
		self.wire_arg_uint32(WL_DISPLAY, WL_DISPLAY_SYNC, WL_SYNC_DONE)

		interfaces = dict()

		old_data = b''
		data = self.socket.recv(4096)
		while data:
			data = old_data + data
			while len(data) >= 8:
				obj_id, sizeop = struct.unpack('=II', data[:8])
				size = sizeop >> 16
				op = sizeop & 0xffff
				if len(data) < size:
					break
				arg_data = data[8:size]
				if obj_id == WL_DISPLAY:
					# Ignore error and delete_id events
					pass
				elif obj_id == WL_SYNC_DONE:
					# All interfaces have been announced
					self.socket.shutdown(socket.SHUT_RDWR)
					self.socket.close()
					return interfaces
				elif self.parse_msg(obj_id, op, arg_data, interfaces):
					pass
				else:
					print(f"Unknown message received: obj_id {obj_id} op {op}")
				data = data[size:]
			old_data = data
			data = self.socket.recv(4096)

		wl_socket = os.path.basename(self.socket.getpeername())
		print(f"error in wayland communication with {self.name} @ {wl_socket}\n")
		self.socket.shutdown(socket.SHUT_RDWR)
		self.socket.close()
		return interfaces

if __name__ == '__main__':
	import sys

	runtime_dir = os.getenv('XDG_RUNTIME_DIR')
	if not runtime_dir:
		print("XDG_RUNTIME_DIR not set")
		exit(1)

	def find_wl_sockets(sockets):
		x = 0
		while True:
			try:
				os.stat(os.path.join(runtime_dir, f'wayland-{x}'))
				sockets.append(f'wayland-{x}')
			except FileNotFoundError:
				break
			x += 1

	compositors = dict()
	wl_sockets = sys.argv[1:]
	if not wl_sockets:
		find_wl_sockets(wl_sockets)

	for wl_socket in wl_sockets:
		wl = Wayland(os.path.join(runtime_dir, wl_socket), log=len(wl_sockets) == 1)
		if len(wl_sockets) == 1:
			print("  {:<45s}  {:>2}".format("Interface", "Version"))
			for name, version in sorted(wl.get_interfaces().items()):
				print("  {:<45s}  {:>2}".format(name, version))
			print()
			exit(0)
		compositors[wl_socket] = (wl.name, wl.get_interfaces())

	all_interfaces = set()
	for _, (_, interfaces) in compositors.items():
		all_interfaces |= set(interfaces.items())

	for compositor, (compositor_name, interfaces) in compositors.items():
		missing = all_interfaces - set(interfaces.items())
		for name, version in set(missing):
			if interfaces.get(name, 0) > version:
				missing.remove((name, version))
		if missing:
			print()
			print(f"\x1b[1m  Protocols missing from {compositor_name} @ {compositor}\x1b[m")
		for name, version in sorted(missing):
			own_version = interfaces.get(name, 0)
			print("  {:<45s}  {:>2}  {}".format(name, version,
				f'(has version {own_version})' if own_version else '')
			)
	print()
