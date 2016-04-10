# fakeroot-wrapper

A wrapper for fakeroot utility which makes saved fakeroot environments filename-indexed (instead of
device/inode indexed).

fakeroot has support for saving the state of fake environment into a file when it exits provided by
it's `-s` and `-i` options. However, that file is device/inode number indexed, which means that fake
attributes are mapped to files by their device number and inode numbers. This has a disadvantage: if
files which attributes have been saved are modified, copied or moved to another partition, their
device and inode numbers may change, and that makes environment state file invalid.

This wrapper program tries to remove this limitation. It converts fakeroot environment file into
it's own format, which is filename-indexed, when fakeroot exits, and then converts it back when you
want to run fakeroot again. This is done by traversing the whole filesystem sub-tree within
specified directory before and after fakeroot is run and converting device/inode numbers from native
fakeroot environment file to relative filenames and vice verse.

## Usage instructions

fakeroot-wrapper accepts several options of it's own, none of which are required, and passes
everything after a special `--` option (or starting with the first non-option argument) to fakeroot.
It also normally exits with exit code of fakeroot which, in turn, exits with exit code of command
that was executed.

The simplest invocation is with no arguments:

    $ fakeroot-wrapper
    # whoami
    root

You can also specify command to run:

    $ fakeroot-wrapper whoami
    root

You can pass additional options to fakeroot by separating them from fakeroot-wrapper options with a
special `--` option:

    $ fakeroot-wrapper -r ls -ln foo/bar
    -rw-rw-r-- 1 0 0 0 Feb 15 21:43 foo/bar
    $ fakeroot-wrapper -r -- -u ls -ln foo/bar
    -rw-rw-r-- 1 1000 10000 0 Feb 15 21:43 foo/bar

By default fakeroot-wrapper will scan current working directory (and it's sub-directories) when
converting native environment file to filename-indexed and vice verse. But you can change that with
the `-d directory` option. The filename of fakeroot environment file to be read and updated can be
specified with `-f fakerootenv_file` option. If no `-f` option is specified then fakeroot-wrapper
derives fakeroot environment filename from the name of directory to be scanned.

    $ ls
    $ fakeroot-wrapper -d foo
    # mkdir foo
    # touch foo/barbaz
    # chown 123:456 foo/barbaz
    # exit
    exit
    $ ls -Rln
    .:
    total 8
    drwxrwsr-x+ 2 1000 10000 4096 Apr 10 20:40 foo/
    -rw-rw-r--  1 1000 10000   87 Apr 10 20:40 foo.fakerootenv
    
    ./foo:
    total 0
    -rw-rw-r-- 1 1000 10000 0 Apr 10 20:40 barbaz
    $ cp -a foo bar
    $ cp foo.fakerootenv bar.fakerootenv
    $ fakeroot-wrapper -d bar ls -ln bar
    итого 0
    -rw-rw-r-- 1 123 456 0 Apr 10 20:40 barbaz

As you can see from this example, the directory specified with `-d` does not have to exist when a
new fakeroot environment is created (i.e. when fakeroot environment file does not exist), but it has
to be created before you exit fakeroot shell (otherwise fakeroot-wrapper will not save anything).
Also, with fakeroot-wrapper you can freely move and copy files, modified inside fakeroot
environment, in most cases, as long as you do not mess with them too much and keep the
`.fakerootenv` file with them.

fakeroot-wrapper has several other useful options. For more information run `fakeroot-wrapper -h`.

## Building and installation

To build fakeroot-wrapper you need a C++11 supporting compiler. Go into the source code directory
and run:

    $ make release

Note that fakeroot-wrapper was tested only on Linux with with GNU Make and g++ compiler. If you have
problems with `make`, something like this will also work:

    $ g++ -std=c++11 fakeroot-wrapper.cpp -O3 -o fakeroot-wrapper

Once you have the binary, copy it to e.g. `/usr/local/bin`, or just use it from where it is. Note
that the `fakeroot` utility must be in your `PATH`.

## Bash-script version

Before fakeroot-wrapper became a C++ program, it was a Bash script. But Bash has proven to be too
slow for this task, so fakeroot-wrapper has been rewritten into C++. However, the now-obsolete
Bash-script implementation is included in this repository too, so you can try it first if you do not
want to bother with building the C++ version. Note that it has much less features and a different
command-line syntax (and no detailed help message). In two words, you should invoke it like this:

    fakeroot-wrapper.sh path/to/directory path/to/file.fakerootenv -- command --to-run

You can pass `-i` option to import native fakeroot environment file or `-n` to disable updating of
fakeroot environment file. Also you can pass additional options to fakeroot after the `--`.

## License

fakeroot-wrapper is licensed under the MIT license. See `LICENSE.txt` for more information.
