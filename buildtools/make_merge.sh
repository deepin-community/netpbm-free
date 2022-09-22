#!/bin/sh

# This program is called by the make files.  It creates the merge.h
# file which is included by pbmmerge.c, etc.

echo "/* File generated by make_merge.sh */" >merge.h
echo "" >>merge.h

echo "#define TRY(s,m) \\" >>merge.h
echo "  { if ( strcmp( cp, s ) == 0 ) exit( m( argc, argv ) ); }" >>merge.h
echo "" >>merge.h

for MERGE_BINARY in $*; do
    echo "TRY( \"${MERGE_BINARY}\", main_${MERGE_BINARY} );" >>merge.h
    done
