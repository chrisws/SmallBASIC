# SmallBASIC
# Copyright(C) 2001-2012 Chris Warren-Smith.
#
# This program is distributed under the terms of the GPL v2.0 or later
# Download the GNU Public License (GPL) from www.gnu.org
# 

# see: http://www.cygwin.com/ml/cygwin/2004-03/msg00289.html
if !WITH_CYGWIN_CONSOLE
  AM_CFLAGS = @PACKAGE_CFLAGS@
endif

AM_CPPFLAGS = -I$(abs_top_builddir)/src -DPACKAGE_LIB_DIR=\""$(pkglibdir)"\"

EXTRA_DIST =                              \
    keywords.c                            \
    ../languages/chars.el.h               \
    ../languages/chars.en.h               \
    ../languages/keywords.el.c            \
    ../languages/keywords.en.c            \
    ../languages/messages.el.h            \
    ../languages/messages.en.h            \
    ../modules/configure.in               \
    ../modules/example1.c                 \
    ../modules/Makefile.am                \
    ../modules/mod_utils.c                \
    ../modules/mod_utils.h                \
    ../modules/example1.c                 \
    ../modules/flite/flite.c              \
    ../modules/flite/Makefile             \
    ../modules/flite/README.TXT           \
    ../modules/flite/test.bas             \
    ../modules/gdbm/gdbm.c                \
    ../modules/gdbm/Makefile              \
    ../modules/mysql/Makefile.am          \
    ../modules/mysql/mysql.c

noinst_LIBRARIES = libsb_common.a

libsb_common_a_SOURCES =                  \
    ../lib/match.c ../lib/match.h         \
    ../lib/jsmn.c ../lib/jsmn.h           \
    ../lib/lodepng.c ../lib/lodepng.h     \
    ../lib/str.c ../lib/str.h             \
    ../lib/matrix.c                       \
    ../lib/xpm.c                          \
    bc.c bc.h                             \
    blib.c blib.h                         \
    blib_db.c                             \
    blib_func.c                           \
    blib_graph.c                          \
    blib_math.c blib_math.h               \
    blib_sound.c                          \
    brun.c                                \
    ceval.c                               \
    device.c device.h                     \
    screen.c                              \
    system.c                              \
    eval.c                                \
    extlib.c extlib.h                     \
    file.c                                \
    ffill.c                               \
    fmt.c fmt.h                           \
    fs_serial.c fs_serial.h               \
    fs_socket_client.c fs_socket_client.h \
    fs_stream.c fs_stream.h               \
    g_line.c                              \
    geom.c geom.h g_bmp.h                 \
    inet.c inet.h                         \
    kw.c kw.h                             \
    pfill.c                               \
    plot.c                                \
    proc.c pproc.h                        \
    sberr.c sberr.h                       \
    scan.c scan.h                         \
    str.c str.h                           \
    tasks.c tasks.h                       \
    hashmap.c hashmap.h                   \
    var_map.c var_map.h                   \
    var_eval.c var_eval.h                 \
    keymap.c keymap.h                     \
    units.c units.h                       \
    var.c var.h                           \
    messages.h                            \
    osd.h                                 \
    sbapp.h                               \
    smbas.h                               \
    sys.h


