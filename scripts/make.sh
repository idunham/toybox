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

echo "generated/newtoys.h"

echo "USE_TOYBOX(NEWTOY(toybox, NULL, TOYFLAG_STAYROOT))" > generated/newtoys.h
sed -n -e 's/^USE_[A-Z0-9_]*(/&/p' toys/*/*.c \
	| sed 's/\(.*TOY(\)\([^,]*\),\(.*\)/\2 \1\2,\3/' | sort -k 1,1 \
	| sed 's/[^ ]* //'  >> generated/newtoys.h
sed -n 's/.*(NEWTOY(\([^,]*\), *\("[^,]*"\) *,.*/#define OPTSTR_\1\t\2/p' \
  generated/newtoys.h > generated/oldtoys.h

# Extract list of command letters from processed header file

function getflags()
{
  FLX="$1"
  shift
  sed -n -e "s/.*TOY($FLX"',[ \t]*"\([^"]*\)"[ \t]*,.*)/\1/' \
         -e 't keep;d;:keep' -e 's/^[<>=][0-9]//' -e 's/[?&^]//' \
         -e 't keep' -e 's/[><=][0-9][0-9]*//g' -e 's/+.//g' \
         -e 's/\[[^]]*\]//g' -e 's/[-?^:&#|@* ;]//g' "$@" -e 'p'
}

# Extract global structure definitions and flag definitions from toys/*/*.c

function getglobals()
{
  # Run newtoys.h through the compiler's preprocessor to resolve USE macros
  # against current config.
  NEWTOYS="$(cat generated/config.h generated/newtoys.h | $CC -E - | sed 's/" *"//g')"

  # Grab allyesconfig for comparison
  ALLTOYS="$((sed '/USE_.*([^)]*)$/s/$/ __VA_ARGS__/' generated/config.h && cat generated/newtoys.h) | $CC -E - | sed 's/" *"//g')"

  for i in toys/*/*.c
  do
    NAME="$(echo $i | sed 's@.*/\(.*\)\.c@\1@')"

    echo -e "// $i\n"
    sed -n -e '/^GLOBALS(/,/^)/b got;b;:got' \
        -e 's/^GLOBALS(/struct '"$NAME"'_data {/' \
        -e 's/^)/};/' -e 'p' $i

    LONGFLAGS="$(echo "$NEWTOYS" | getflags "$NAME" -e 's/\(\(([^)]*)\)*\).*/\1/' -e 's/(//g' -e 's/)/ /g')"
    FLAGS="$(echo "$NEWTOYS" | getflags "$NAME" -e 's/([^)]*)//g')"
    ZFLAGS="$(echo "$ALLTOYS" | getflags "$NAME" -e 's/([^)]*)//g' -e 's/[-'"$FLAGS"']//g')"
    LONGFLAGLEN="$(echo "$LONGFLAGS" | wc -w)"

    echo "#ifdef FOR_${NAME}"
    X=0
    # Provide values for --longopts with no corresponding short flags
    for i in $LONGFLAGS
    do
      X=$(($X+1))
      echo -e "#define FLAG_$i\t(1<<$(($LONGFLAGLEN+${#FLAGS}-$X)))"
    done

    # Provide values for active flags
    X=0
    while [ $X -lt ${#FLAGS} ]
    do
      echo -ne "#define FLAG_${FLAGS:$X:1}\t"
      X=$(($X+1))
      echo "(1<<$((${#FLAGS}-$X)))"
    done

    # Provide zeroes for inactive flags
    X=0
    while [ $X -lt ${#ZFLAGS} ]
    do
      echo "#define FLAG_${ZFLAGS:$X:1} 0"
      X=$(($X+1))
    done
    echo "#define TT this.${NAME}"
    echo "#endif"
  done
}

echo "generated/globals.h"

GLOBSTRUCT="$(getglobals)"
(
  echo "$GLOBSTRUCT"
  echo
  echo "extern union global_union {"
  echo "$GLOBSTRUCT" | sed -n 's/struct \(.*\)_data {/	struct \1_data \1;/p'
  echo "} this;"
) > generated/globals.h

echo "generated/help.h"
# Only recreate generated/help.h if python2 is installed. Does not work with 3.
PYTHON="$(which python2 || which python2.6 || which python2.7)"
if [ ! -z "$(grep 'CONFIG_TOYBOX_HELP=y' $KCONFIG_CONFIG)" ];
then
  if [ -z "$PYTHON" ];
  then
    echo "Python 2.x required to rebuild generated/help.h"
    # exit 1
  else
    echo "Extract help text from Config.in."
    "$PYTHON" scripts/config2help.py Config.in > generated/help.h || exit 1
  fi
fi

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
