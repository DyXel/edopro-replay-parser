project('edopro-replay-parser', ['c', 'cpp'], license : 'AGPL-3.0-or-later', default_options : 'cpp_std=c++17')

ygopen_dep = dependency('ygopen')

erp_src = files([
	'src/main.cpp',
	'src/parser.cpp',
	'src/LZMA/Alloc.c',
	'src/LZMA/LzmaDec.c'
])

erp_exe = executable('erp', erp_src, c_args: '-D_7ZIP_ST', dependencies : ygopen_dep)
