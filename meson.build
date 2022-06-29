project('edopro-replay-parser', ['c', 'cpp'], license : 'AGPL-3.0-or-later', default_options : 'cpp_std=c++17')

lzma_dep = dependency('liblzma')
ygopen_dep = dependency('ygopen')

erp_src = files([
	'src/main.cpp',
	'src/parser.cpp',
])

erp_exe = executable('erp', erp_src, dependencies : [lzma_dep, ygopen_dep])
