vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'github_git': 'https://github.com',
  'dawn_git': 'https://dawn.googlesource.com',
  'dawn_revision': '5987c4e8395aed194c987c051b56d35485180b46',
  'angle_root': 'third_party/angle',
  'angle_revision': '4d153383bd3a2359a44fb0c13a65119c0087702f',

  'clang_format_revision': '96636aa0e9f047f17447f2d45a094d0b59ed7917',
  "libcxx_revision": "a50f5035629b7621e92acef968403f71b7d48553",
  "libcxxabi_revision": "0d529660e32d77d9111912d73f2c74fc5fa2a858",
  "libunwind_revision": "69d9b84cca8354117b9fe9705a4430d789ee599b",
}

deps = {
  # Dependencies required to use GN/Clang in standalone
  'build': {
    'url': '{chromium_git}/chromium/src/build@9a53be87ebb636c35f2ed9772e5deaeb350d790b',
  },
  'buildtools': {
    'url': '{chromium_git}/chromium/buildtools.git@6fbda1b24c1893a893b17aa219b765b9e7c801d8',
  },
  'tools/clang': {
    'url': '{chromium_git}/chromium/src/tools/clang@3114fbc11f9644c54dd0a4cdbfa867bac50ff983',
  },
  'third_party/binutils': {
    'url': '{chromium_git}/chromium/src/third_party/binutils@2be73f7fbf783d7a0b288e174a5773b67c7656bc',
  },

  'testing': {
    'url': '{chromium_git}/chromium/src/testing@b07830f6905ce9e33034ad14820bc0a58b6e9e41',
  },
  'third_party/googletest': {
    'url': '{chromium_git}/external/github.com/google/googletest@5ec7f0c4a113e2f18ac2c6cc7df51ad6afc24081',
  },

  'third_party/stb': {
    'url': '{github_git}/nothings/stb.git@c7110588a4d24c4bb5155c184fbb77dd90b3116e',
  },
  'third_party/glfw': {
    'url': '{chromium_git}/external/github.com/glfw/glfw@2de2589f910b1a85905f425be4d32f33cec092df',
  },
  'third_party/glm': {
    'url': '{github_git}/g-truc/glm.git@06f084063fd6d9aa2ef6904517650700ae47b63d',
  },
  'third_party/rapidjson': {
    'url': '{github_git}/Tencent/rapidjson.git',
  },
  'third_party/shaderc': {
    'url': '{chromium_git}/external/github.com/google/shaderc@634dd3545cbccb9362f16f41b3b75703f290a9fd',
  },
  'third_party/dawn': {
    'url': '{dawn_git}/dawn.git@{dawn_revision}',
  },
  'third_party/angle': {
    'url': '{chromium_git}/angle/angle.git@{angle_revision}',
  }
}

hooks = [
  # Pull GN binaries using checked-in hashes.
  {
    'name': 'gn_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/linux64/gn.sha1',
    ],
  },

  # Pull the compilers and system libraries for hermetic builds
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and ((checkout_x86 or checkout_x64))',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x64)',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Update the Windows toolchain if necessary. Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'tools/clang/scripts/update.py'],
    'condition': 'dawn_standalone',
  },
  {
    # Pull rc binaries using checked-in hashes.
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and (host_os == "win")',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  # Pull binutils for linux hermetic builds
  {
    'name': 'binutils',
    'pattern': 'src/third_party/binutils',
    'condition': 'host_os == "linux"',
    'action': [
        'python',
        'third_party/binutils/download.py',
    ],
  },
  # Update build/util/LASTCHANGE.
  {
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi
  'buildtools',
  'third_party/angle',
  'third_party/dawn',
]
