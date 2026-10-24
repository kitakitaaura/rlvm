// On-demand game files for the rlvm web build.
//
// rlvm reads files synchronously (fopen, ifstream, open + mmap), so the web
// build used to copy the whole game into MEMFS before starting. A 5 GB game
// does not fit in a phone's tab, which the browser then kills.
//
// Instead, the runtime page (noberu-web/rlvm-runtime/rlvm.html) creates every
// game file as a placeholder that reports its real size but holds no data,
// and sets Module.lazyFS. This replaces the openat syscall: opening a
// placeholder first has Module.lazyFS.loadSync() read that one file in, then
// the open proceeds as normal. Everything else - stat, read, mmap, close - is
// plain MEMFS, since by the time anything reads, the data is there.
//
// The read is synchronous (see rlvm.html), not ASYNCIFY: rlvm is built with
// Emscripten's JS-based C++ exceptions, whose invoke_* trampolines ASYNCIFY
// cannot unwind through, so suspending inside an open traps.
//
// This replaces libsyscall.js's __syscall_openat, which is normally wrapped
// by wrapSyscallFunction (varargs setup, ErrnoError to -errno). A library
// added later is not wrapped, so both are done by hand here.

addToLibrary({
  __syscall_openat__deps: ['$syscallGetVarargI', '$FS', '$SYSCALLS'],
  __syscall_openat: function (dirfd, path, flags, varargs) {
    try {
      SYSCALLS.varargs = varargs;
      path = SYSCALLS.getStr(path);
      path = SYSCALLS.calculateAt(dirfd, path);
      var mode = varargs ? syscallGetVarargI() : 0;
      if (flags & {{{ cDefs.O_CREAT }}}) {
        mode &= ~SYSCALLS.currentUmask;
      }
      var lazy = Module['lazyFS'];
      if (lazy && lazy.needs(path)) {
        try {
          lazy.loadSync(path);
        } catch (e) {
          err('lazyFS: could not load ' + path + ': ' + e);
          return -{{{ cDefs.EIO }}};
        }
      }
      var stream = FS.open(path, flags, mode);
      if (lazy) lazy.opened(stream, flags);
      return stream.fd;
    } catch (e) {
      if (typeof FS == 'undefined' || e.name !== 'ErrnoError') throw e;
      return -e.errno;
    }
  },
});
