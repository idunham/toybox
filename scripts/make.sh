#!/bin/bash

# Grab default values for $CFLAGS and such.

export LANG=c
source ./configure

[ -z "$KCONFIG_CONFIG" ] && KCONFIG_CONFIG=".config"

if [ -z "$KCONFIG_CONFIG" ]
then
  echo "No $KCONFIG_CONFIG (see "make help" for configuration options)."
  exit 1
fi

# Respond to V= by echoing command lines as well as running them
do_loudly()
{
  [ ! -z "$V" ] && echo "$@"
  "$@"
}

echo "Make generated/config.h from $KCONFIG_CONFIG."

# This long and roundabout sed invocation is to make old versions of sed happy.
# New ones have '\n' so can replace one line with two without all the branches
# and tedious mucking about with hold space.

sed -n \
  -e 's/^# CONFIG_\(.*\) is not set.*/\1/' \
  -e 't notset' \
  -e 's/^CONFIG_\(.*\)=y.*/\1/' \
  -e 't isset' \
  -e 's/^CONFIG_\([^=]*\)=\(.*\)/#define CFG_\1 \2/p' \
  -e 'd' \
  -e ':notset' \
  -e 'h' \
  -e 's/.*/#define CFG_& 0/p' \
  -e 'g' \
  -e 's/.*/#define USE_&(...)/p' \
  -e 'd' \
  -e ':isset' \
  -e 'h' \
  -e 's/.*/#define CFG_& 1/p' \
  -e 'g' \
  -e 's/.*/#define USE_&(...) __VA_ARGS__/p' \
  $KCONFIG_CONFIG > generated/config.h || exit 1


echo "Extract configuration information from toys/*.c files..."
scripts/genconfig.sh

echo "Generate headers from toys/*/*.c..."

# Create a list of all the commands toybox can provide. Note that the first
# entry is out of order on purpose (the toybox multiplexer command must be the
# first element of the array). The rest must be sorted in alphabetical order
# for fast binary search.

echo -n "generated/newtoys.h "

echo "USE_TOYBOX(NEWTOY(toybox, NULL, TOYFLAG_STAYROOT))" > generated/newtoys.h
sed -n -e 's/^USE_[A-Z0-9_]*(/&/p' toys/*/*.c \
	| sed 's/\(.*TOY(\)\([^,]*\),\(.*\)/\2 \1\2,\3/' | sort -k 1,1 \
	| sed 's/[^ ]* //'  >> generated/newtoys.h
sed -n -e 's/.*(NEWTOY(\([^,]*\), *\(\("[^"]*"[^,]*\)*\),.*/#define OPTSTR_\1\t\2/p' \
  generated/newtoys.h > generated/oldtoys.h

do_loudly $HOSTCC scripts/mkflags.c -o generated/mkflags || exit 1

echo -n "generated/flags.h "

# Parse files through C preprocessor twice, once to get flags for current
# .config and once to get flags for allyesconfig
for I in A B
do
  (
  # define macros and select header files with option string data

  echo "#define NEWTOY(aa,bb,cc) aa $I bb"
  echo '#define OLDTOY(...)'
  if [ "$I" == A ]
  then
    cat generated/config.h
  else
    sed '/USE_.*([^)]*)$/s/$/ __VA_ARGS__/' generated/config.h
  fi
  cat generated/newtoys.h

  # Run result through preprocessor, glue together " " gaps leftover from USE
  # macros, delete comment lines, print any line with a quoted optstring,
  # turn any non-quoted opstring (NULL or 0) into " " (because fscanf can't
  # handle "" with nothing in it).

  ) | ${CROSS_COMPILE}${CC} -E - | \
    sed -n -e 's/" *"//g;/^#/d;s/"/"/p' -e 's/ *$//;s/ [^" ]*$/ " "/p'

# Sort resulting line pairs and glue them together into triplets of
#   command "flags" "allflags"
# to feed into mkflags C program that outputs actual flag macros

done | sort | sed -n 's/ A / /;t skip;d;:skip;h;n;s/[^ ]* B //;H;g;s/\n/ /;p' |\
generated/mkflags > generated/flags.h || exit 1

# Extract global structure definitions and flag definitions from toys/*/*.c

function getglobals()
{
  for i in toys/*/*.c
  do
    NAME="$(echo $i | sed 's@.*/\(.*\)\.c@\1@')"
    DATA="$(sed -n -e '/^GLOBALS(/,/^)/b got;b;:got' \
            -e 's/^GLOBALS(/struct '"$NAME"'_data {/' \
            -e 's/^)/};/' -e 'p' $i)"

    [ ! -z "$DATA" ] && echo -e "// $i\n\n$DATA\n"
  done
}

echo -n "generated/globals.h "

GLOBSTRUCT="$(getglobals)"
(
  echo "$GLOBSTRUCT"
  echo
  echo "extern union global_union {"
  echo "$GLOBSTRUCT" | sed -n 's/struct \(.*\)_data {/	struct \1_data \1;/p'
  echo "} this;"
) > generated/globals.h

echo "generated/help.h"
do_loudly $HOSTCC scripts/config2help.c -I . lib/xwrap.c lib/llist.c lib/lib.c \
  -o generated/config2help && \
generated/config2help Config.in $KCONFIG_CONFIG > generated/help.h || exit 1

# Extract a list of toys/*/*.c files to compile from the data in $KCONFIG_CONFIG

# 1) Get a list of C files in toys/* and glue them together into a regex we can
# feed to grep that will match any one of them (whole word, not substring).
TOYFILES="^$(ls toys/*/*.c | sed -n 's@^.*/\(.*\)\.c$@\1@;s/-/_/g;H;${g;s/\n//;s/\n/$|^/gp}')\$"

# 2) Grab the XXX part of all CONFIG_XXX entries, removing everything after the
# second underline
# 3) Sort the list, keeping only one of each entry.
# 4) Convert to lower case.
# 5) Remove any config symbol not recognized as a filename from step 1.
# 6) Add "toys/*/" prefix and ".c" suffix.

TOYFILES=$(sed -nre 's/^CONFIG_(.*)=y/\1/p' < "$KCONFIG_CONFIG" \
  | sort -u | tr A-Z a-z | grep -E "$TOYFILES" | sed 's@\(.*\)@toys/\*/\1.c@')

echo "Library probe..."

# We trust --as-needed to remove each library if we don't use any symbols
# out of it, this loop is because the compiler has no way to ignore a library
# that doesn't exist, so we have to detect and skip nonexistent libraries
# for it.

OPTLIBS="$(for i in util crypt m; do echo "int main(int argc, char *argv[]) {return 0;}" | ${CROSS_COMPILE}${CC} -xc - -o /dev/null -Wl,--as-needed -l$i > /dev/null 2>/dev/null && echo -l$i; done)"

echo "Compile toybox..."

do_loudly()
{
  [ ! -z "$V" ] && echo "$@"
  "$@"
}

do_loudly ${CROSS_COMPILE}${CC} $CFLAGS -I . -o toybox_unstripped $OPTIMIZE \
  main.c lib/*.c $TOYFILES -Wl,--as-needed $OPTLIBS  || exit 1
do_loudly ${CROSS_COMPILE}${STRIP} toybox_unstripped -o toybox || exit 1
# gcc 4.4's strip command is buggy, and doesn't set the executable bit on
# its output the way SUSv4 suggests it do so.
do_loudly chmod +x toybox || exit 1
