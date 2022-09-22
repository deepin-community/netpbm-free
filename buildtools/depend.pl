#! /usr/bin/perl -w

@pbm=@ppm=@pgm=@pnm=();
@ppm = ("libppm1", "libppm2", "libppmcmap", "libppm4",
"libppm5", "libppmfloyd");
@pbm = ("libpm", "libpbm1", "libpbm2", "libpbm3", "libpbm4",
"libpbm5", "bitio");
@pgm = ("libpgm1", "libpgm2");
@pnm = ("libpnm1", "libpnm2", "libpnm3", "libpnm4", "libpam", "libpammap");
%min = ('pbm' => 25, 'pgm' => 19, 'pnm' => 25, 'ppm' => 25);
%extradep = ('pbm' => '', 'pgm' => 'lib/shared/libpbm.so', 
	'pnm' => 'lib/shared/libppm.so', 'ppm' => 'lib/shared/libpgm.so');
%extra = ('pbm' => '', 'pgm' => '-lpbm', 
	'pnm' => '-lppm -lpgm -lpbm', 'ppm' => '-lpbm -lpgm');

foreach $i ('ppm', 'pbm', 'pgm', 'pnm') {
	foreach $j (@$i) {
		print "lib/shared/$j.o: $i/$j.c shhopt/netpbm-shhopt.h\n";
		print "\tmkdir -p lib/shared\n";
		print "\t\$(CC) -c \$(INCLUDE) -I \$(SRCDIR)/include/ ".
			"-I \$(SRCDIR)/shhopt/ ".
			"\$(CFLAGS) -fPIC -D_REENTRANT \$(CDEBUG) -o \$@ \$<\n\n";
		print "lib/static/$j.o: $i/$j.c shhopt/netpbm-shhopt.h\n";
		print "\tmkdir -p lib/static\n";
		print "\t\$(CC) -c \$(INCLUDE) -I \$(SRCDIR)/include/ ".
			"-I \$(SRCDIR)/shhopt/ ".
			"\$(CFLAGS) -D_REENTRANT \$(CDEBUG) -o \$@ \$<\n\n";
		print "lib/static/lib$i.a: lib/static/$j.o\n";
		print "lib/static/libnetpbm.a: lib/static/$j.o\n";
		print "lib/shared/lib$i.so.9.$min{$i}: lib/shared/$j.o\n";
		print "lib/shared/libnetpbm.so.10.0: lib/shared/$j.o\n";
	}
}

print "lib/static/libpbm.a: lib/static/shhopt.o\n";
print "lib/static/libnetpbm.a: lib/static/shhopt.o\n";
print "lib/shared/libpbm.so.9.$min{'pbm'}: lib/shared/shhopt.o\n";
print "lib/shared/libnetpbm.so.10.0: lib/shared/shhopt.o\n";

print "lib/shared/shhopt.o: shhopt/shhopt.c shhopt/netpbm-shhopt.h\n";
print "\tmkdir -p lib/shared\n";
print "\t\$(CC) -c \$(INCLUDE) -I \$(SRCDIR)/include/ ".
	"-I \$(SRCDIR)/shhopt/ ".
	"\$(CFLAGS) -fPIC -D_REENTRANT \$(CDEBUG) -o \$@ \$<\n\n";
print "lib/static/shhopt.o: shhopt/shhopt.c shhopt/netpbm-shhopt.h\n";
print "\tmkdir -p lib/static\n";
print "\t\$(CC) -c \$(INCLUDE) -I \$(SRCDIR)/include/ ".
	"-I \$(SRCDIR)/shhopt/ ".
	"\$(CFLAGS) -D_REENTRANT \$(CDEBUG) -o \$@ \$<\n\n";

print "lib/static/libpbmvms.o: pbm/libpbmvms.c shhopt/netpbm-shhopt.h\n";
print "\tmkdir -p lib/static\n";
print "\t\$(CC) -c \$(INCLUDE) \$(CFLAGS) \$(CDEBUG) -o \$@ \$<\n\n";
print "lib/shared/libpbmvms.o: pbm/libpbmvms.c shhopt/netpbm-shhopt.h\n";
print "\tmkdir -p lib/shared\n";
print "\t\$(CC) -c \$(INCLUDE) \$(CFLAGS) \$(CDEBUG) -o \$@ \$<\n\n";


foreach $i ('ppm', 'pbm', 'pgm', 'pnm') {
	print "LIBOBJECT_$i := ".join('.o ', @$i).".o\n";
}

print "ifneq (\${VMS}x,x)\nLIBOBJECT_pbm += libpbmvms.o\n".
	"\nlib/static/libpbm.a: lib/static/libpbmvms.o\n".
	"\nlib/shared/libpbm.a: lib/shared/libpbmvms.o\n".
	"\nendif\n\n";


print "lib/static/lib%.a:\n".
	"\tar cq \$@ \$+\n\tranlib \$@\n\n";
print "lib/static/libnetpbm.a:\n".
	"\tar cq \$@ \$+\n\tranlib \$@\n\n";

foreach $i ('ppm', 'pbm', 'pgm', 'pnm') {
	print "lib/shared/lib$i.so.9.$min{$i}: $extradep{$i}\n\n";
	print "lib/shared/lib$i.so.9.$min{$i}:\n".
		"\t\$(LD) \$(LDSHLIB) -Llib/shared -o \$@ \$+ $extra{$i} -lc \$(CDEBUG)\n\n";
	print "lib/shared/lib$i.so.9.$min{$i}: SONAME := lib$i.so.9\n";
	print "lib/shared/lib$i.so.9: lib/shared/lib$i.so.9.$min{$i}\n".
		"\tln -sf \$\$(basename \$+) \$@\n\n";
	print "lib/shared/lib$i.so: lib/shared/lib$i.so.9\n".
		"\tln -sf \$\$(basename \$+) \$@\n\n";
	print "install.lib.old-shared.lib:: lib/shared/lib$i.so.9.$min{$i}\n".
		"\t\$(INSTALL) -c -m \$(INSTALL_PERM_LIBS) \$<".
			" \$(INSTALLSTATICLIBS)/lib$i.so.9.$min{$i}\n";
	print "install.lib.old-shared.lib::\n".
		"\t\$(SYMLINK) lib$i.so.9.$min{$i} \$(INSTALLSTATICLIBS)/lib$i.so.9\n";
	print "install.lib.old-shared.devel::\n".
		"\t\$(SYMLINK) lib$i.so.9 \$(INSTALLSTATICLIBS)/lib$i.so\n";
	print "install.lib.old-static:: lib/static/lib$i.a\n".
		"\t\$(INSTALL) -c -m \$(INSTALL_PERM_LIBS) \$<".
			" \$(INSTALLSTATICLIBS)/lib$i.a\n";
}

print "lib/shared/libnetpbm.so.10.0: SONAME := libnetpbm.so.10\n";
print "lib/shared/libnetpbm.so.10.0:\n".
	"\t\$(LD) \$(LDSHLIB) -o \$@ \$+ -lc \$(CDEBUG)\n\n";
print "lib/shared/libnetpbm.so.10: lib/shared/libnetpbm.so.10.0\n".
	"\tln -sf \$\$(basename \$+) \$@\n\n";
print "lib/shared/libnetpbm.so: lib/shared/libnetpbm.so.10\n".
	"\tln -sf \$\$(basename \$+) \$@\n\n";
	print "install.lib.shared.lib:: lib/shared/libnetpbm.so.10.0\n".
		"\t\$(INSTALL) -c -m \$(INSTALL_PERM_LIBS) \$<".
			" \$(INSTALLSTATICLIBS)/libnetpbm.so.10.0\n";
	print "install.lib.shared.lib::\n".
		"\t\$(SYMLINK) libnetpbm.so.10.0 \$(INSTALLSTATICLIBS)/libnetpbm.so.10\n";
	print "install.lib.shared.devel::\n".
		"\t\$(SYMLINK) libnetpbm.so.10 \$(INSTALLSTATICLIBS)/libnetpbm.so\n";
	print "install.lib.static:: lib/static/libnetpbm.a\n".
		"\t\$(INSTALL) -c -m \$(INSTALL_PERM_LIBS) \$<".
			" \$(INSTALLSTATICLIBS)/libnetpbm.a";

