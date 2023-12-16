project('edopro-replay-parser',
	['cpp'],
	license : 'AGPL-3.0-or-later',
	default_options : 'cpp_std=c++17'
)

lzma_dep = dependency('liblzma')
ygopen_dep = dependency('ygopen')

erp_src = files(
	'src/decompress.cpp',
	'src/main.cpp',
	'src/parser.cpp',
	'src/print_date.cpp',
	'src/print_names.cpp',
)

erp_exe = executable('erp', erp_src, dependencies : [lzma_dep, ygopen_dep])
