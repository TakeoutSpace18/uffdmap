project('uffdmap', 'c',
  default_options: ['warning_level=3']
)

uring = dependency('liburing')

uffdmap_shared = shared_library('uffdmap',
   ['uffdmap.c', 'mmap_hack.c'],
   dependencies: [uring]
)

uffdmap = declare_dependency(
  link_with: [uffdmap_shared],
  include_directories: [include_directories('.')]
)

test('tst-uffdmap',
  executable('tst-uffdmap',
    ['tst-uffdmap.c'],
    dependencies: [uffdmap]
  )
)

uffdmap_cat = executable('uffdmap-cat',
  ['uffdmap-cat.c'],
  dependencies: [uffdmap]
)

