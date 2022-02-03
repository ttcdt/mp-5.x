/*

    MPSL - Minimum Profit Scripting Language
    Function library

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <math.h>
#include <time.h>

#include "mpdm.h"
#include "mpsl.h"

/** code **/

#define F_ARGS mpdm_t a, mpdm_t l

#define A(n) mpdm_get_i(a, n)
#define A0 A(0)
#define A1 A(1)
#define A2 A(2)
#define A3 A(3)
#define A4 A(4)
#define IA(n) mpdm_ival(A(n))
#define IA0 IA(0)
#define IA1 IA(1)
#define IA2 IA(2)
#define IA3 IA(3)
#define RA(n) mpdm_rval(A(n))
#define RA0 RA(0)
#define RA1 RA(1)
#define RA2 RA(2)
#define RA3 RA(3)

/**
 * size - Returns the size of a value.
 * @v: the value
 *
 * Returns the size of a value. Its meaning may not be totally
 * obvious; use count() instead for all type of values.
 * [Value Management]
 */
/** integer = size(v); */
/* ; */
static mpdm_t F_size(F_ARGS)
{
    return MPDM_I(mpdm_size(A0));
}

/**
 * clone - Creates a clone of a value.
 * @v: the value
 *
 * Creates a clone of a value. If the value is multiple, a new value will
 * be created containing clones of all its elements; otherwise,
 * the same unchanged value is returned.
 * [Value Management]
 */
/** v2 = clone(v); */
static mpdm_t F_clone(F_ARGS)
{
    return mpdm_clone(A0);
}

/**
 * dump - Dumps a value to stdin.
 * @v: The value
 *
 * Dumps a value to stdin. The value can be complex. This function
 * is for debugging purposes only.
 * [Debugging]
 * [Input-Output]
 */
/** dump(v); */
static mpdm_t F_dump(F_ARGS)
{
    mpdm_dump(A0);
    return NULL;
}

/**
 * dumper - Returns a visual representation of a complex value.
 * @v: The value
 *
 * Returns a visual representation of a complex value.
 * [Debugging]
 * [Strings]
 */
/** string = dumper(v); */
static mpdm_t F_dumper(F_ARGS)
{
    return mpdm_dumper(A0);
}

/**
 * cmp - Compares two values.
 * @v1: the first value
 * @v2: the second value
 *
 * Compares two values. If both are strings, a standard string
 * comparison (using wcscmp()) is returned; if both are arrays,
 * the size is compared first and, if they have the same number
 * elements, each one is compared; otherwise, a simple pointer
 * comparison is done.
 *
 * In either case, an integer is returned, which is < 0 if @v1
 * is lesser than @v2, > 0 on the contrary or 0 if both are
 * equal.
 * [Strings]
 * [Arrays]
 */
/** integer = cmp(v); */
static mpdm_t F_cmp(F_ARGS)
{
    return MPDM_I(mpdm_cmp(A0, A1));
}

/**
 * is_exec - Tests if a value is executable.
 * @v: the value
 *
 * Returns non-zero if @v is a executable.
 * [Value Management]
 */
/** bool = is_exec(v); */
static mpdm_t F_is_exec(F_ARGS)
{
    return mpdm_bool(mpdm_can_exec(A0));
}

static mpdm_t F_splice(F_ARGS)
{
    mpdm_t n;

    return mpdm_splice(A0, A1, IA2, IA3, &n, NULL);
}

static mpdm_t F_slice(F_ARGS)
{
    mpdm_t d;

    return mpdm_splice(A0, NULL, IA1, IA2, NULL, &d);
}

static mpdm_t F_reverse(F_ARGS)
{
    return mpdm_reverse(A0);
}

static mpdm_t F_bool(F_ARGS)
{
    return mpdm_bool(mpdm_is_true(A0));
}


static mpdm_t F_escape(F_ARGS)
{
    mpdm_t v, low, high, f;

    v    = A0;
    low  = A1;
    high = A2;
    f    = A3;

    return mpdm_escape(v, mpdm_string(low)[0], mpdm_string(high)[0], f);
}


/**
 * expand - Expands an array.
 * @a: the array
 * @offset: insertion offset
 * @num: number of elements to insert
 *
 * Expands an array value, inserting @num elements (initialized
 * to NULL) at the specified @offset.
 * [Arrays]
 */
/** expand(a, offset, num); */
static mpdm_t F_expand(F_ARGS)
{
    return mpdm_expand(A0, IA1, IA2);
}

/**
 * collapse - Collapses an array.
 * @a: the array
 * @offset: deletion offset
 * @num: number of elements to collapse
 *
 * Collapses an array value, deleting @num elements at
 * the specified @offset.
 * [Arrays]
 */
/** collapse(a, offset, num); */
static mpdm_t F_collapse(F_ARGS)
{
    return mpdm_collapse(A0, IA1, IA2);
}

/**
 * ins - Insert an element in an array.
 * @a: the array
 * @e: the element to be inserted
 * @offset: subscript where the element is going to be inserted
 *
 * Inserts the @e value in the @a array at @offset.
 * Further elements are pushed up, so the array increases its size
 * by one. Returns the inserted element.
 * [Arrays]
 */
/** e = ins(a, e, offset); */
static mpdm_t F_ins(F_ARGS)
{
    return mpdm_ins(A0, A1, IA2);
}

/**
 * shift - Extracts the first element of an array.
 * @a: the array
 *
 * Extracts the first element of the array. The array
 * is shrinked by one.
 *
 * Returns the deleted element.
 * [Arrays]
 */
/** v = shift(a); */
static mpdm_t F_shift(F_ARGS)
{
    return mpdm_shift(A0);
}

/**
 * push - Pushes a value into an array.
 * @a: the array
 * @arg1: first value
 * @arg2: second value
 * @argn: nth value
 *
 * Pushes values into an array (i.e. inserts at the end).
 * Returns the last element pushed.
 * [Arrays]
 */
/** argn = push(a, arg1 [, arg2, ... argn]); */
static mpdm_t F_push(F_ARGS)
{
    int n;
    mpdm_t r = NULL;

    for (n = 1; n < mpdm_size(a); n++) {
        mpdm_unref(r);
        r = mpdm_push(A0, A(n));
        mpdm_ref(r);
    }

    return mpdm_unrefnd(r);
}

/**
 * pop - Pops a value from an array.
 * @a: the array
 *
 * Pops a value from the array (i.e. deletes from the end
 * and returns it).
 * [Arrays]
 */
/** v = pop(a); */
static mpdm_t F_pop(F_ARGS)
{
    return mpdm_pop(A0);
}

/**
 * queue - Implements a queue in an array.
 * @a: the array
 * @e: the element to be pushed
 * @size: maximum size of array
 *
 * Pushes the @e element into the @a array. If the array already has
 * @size elements, the first (oldest) element is deleted from the
 * queue and returned.
 *
 * Returns the deleted element, or NULL if the array doesn't have
 * @size elements yet.
 * [Arrays]
 */
/** v = queue(a, e, size); */
static mpdm_t F_queue(F_ARGS)
{
    return mpdm_queue(A0, A1, IA2);
}

/**
 * seek - Seeks a value in an array (sequential).
 * @a: the array
 * @k: the key
 * @step: number of elements to step
 *
 * Seeks sequentially the value @k in the @a array in
 * increments of @step. A complete search should use a step of 1.
 * Returns the offset of the element if found, or -1 otherwise.
 * [Arrays]
 */
/** integer = seek(a, k, step); */
static mpdm_t F_seek(F_ARGS)
{
    return MPDM_I(mpdm_seek(A0, A1, IA2));
}

/**
 * sort - Sorts an array.
 * @a: the array
 * @sorting_func: sorting function
 *
 * Sorts the array. For each pair of elements being sorted, the
 * @sorting_func is called with the two elements to be sorted as
 * arguments. This function must return a signed integer value indicating
 * the sorting order.
 *
 * If no function is supplied, the sorting is done using cmp().
 *
 * Returns the sorted array (the original one is left untouched).
 * [Arrays]
 */
/** array = sort(a); */
/** array = sort(a, sorting_func); */
static mpdm_t F_sort(F_ARGS)
{
    mpdm_t r, v;

    v = mpdm_ref(A0);
    r = mpdm_sort_cb(mpdm_clone(v), 1, A1);
    mpdm_unref(v);

    return r;
}

/**
 * split - Separates a string into an array of pieces.
 * @v: the value to be separated
 * @s: the separator
 *
 * Separates the @v string value into an array of pieces, using @s
 * as a separator.
 *
 * If the separator is NULL, the string is splitted by characters.
 *
 * If the string does not contain the separator, an array holding 
 * the complete string as its unique argument is returned.
 * [Arrays]
 * [Strings]
 */
/** array = split(v, s); */
static mpdm_t F_split(F_ARGS)
{
    return mpdm_split(A0, A1);
}

/**
 * join - Joins an array.
 * @a: array to be joined
 * @s: joiner string or second array
 *
 * If @s is a string or NULL, returns a new string with all elements
 * in @a joined using @s. If @s is an array, it returns a new one
 * containing all elements of @a followed by all elements of @s.
 * [Arrays]
 * [Strings]
 */
/** string = join(a, joiner_str); */
/** array = join(a1, a2); */
static mpdm_t F_join(F_ARGS)
{
    return mpdm_join(A0, A1);
}


/**
 * exists - Tests if a key exists.
 * @h: the hash
 * @k: the key
 *
 * Returns 1 if @k is defined in @h, or 0 othersize.
 * [Hashes]
 */
/** bool = exists(h, k); */
static mpdm_t F_exists(F_ARGS)
{
    return mpdm_bool(mpdm_exists(A0, A1));
}


/**
 * open - Opens a file.
 * @filename: the file name
 * @mode: an fopen-like mode string
 *
 * Opens a file. If @filename can be open in the specified @mode, a
 * value will be returned containing the file descriptor, or NULL
 * otherwise.
 *
 * If the file is open for reading, some charset detection methods are
 * used. If any of them is successful, its name is stored in the
 * `DETECTED_ENCODING' global variable. This value is
 * suitable to be copied over `ENCODING' or `TEMP_ENCODING'.
 *
 * If the file is open for writing, the encoding to be used is read from
 * the `ENCODING' global variable and, if not set, from the
 * `TEMP_ENCODING' one. The latter will always be deleted afterwards.
 * [Input-Output]
 * [Character Set Conversion]
 */
/** fd = open(filename, mode); */
static mpdm_t F_open(F_ARGS)
{
    return mpdm_open(A0, A1);
}

/**
 * close - Closes a file descriptor.
 * @fd: the file descriptor
 *
 * Closes the file descriptor.
 * [Input-Output]
 */
/** close(fd); */
static mpdm_t F_close(F_ARGS)
{
    return MPDM_I(mpdm_close(A0));
}

/**
 * read - Reads a line from a file descriptor.
 * @fd: the file descriptor
 *
 * Reads a line from @fd. Returns the line, or NULL on EOF.
 * [Input-Output]
 * [Character Set Conversion]
 */
/** string = read(fd); */
static mpdm_t F_read(F_ARGS)
{
    return mpdm_read(A0);
}

/**
 * getchar - Reads a character from a file descriptor.
 * @fd: the file descriptor
 *
 * Returns a character read from @fd, or NULL on EOF. No
 * charset conversion is done.
 * [Input-Output]
 */
/** string = getchar(fd); */
static mpdm_t F_getchar(F_ARGS)
{
    return mpdm_getchar(A0);
}

/**
 * putchar - Writes a character to a file descriptor.
 * @fd: the file descriptor
 * @s: the string
 *
 * Writes the first character in @s into @fd. No charset
 * conversion is done.
 *
 * Returns the number of chars written (0 or 1).
 * [Input-Output]
 */
/** s = putchar(fd, s); */
static mpdm_t F_putchar(F_ARGS)
{
    return MPDM_I(mpdm_putchar(A0, A1));
}

/**
 * fseek - Sets a file pointer.
 * @fd: the file descriptor
 * @offset: the offset
 * @whence: the position
 *
 * Sets the file pointer position of @fd to @offset. @whence can
 * be: 0 for SEEK_SET, 1 for SEEK_CUR and 2 for SEEK_END.
 *
 * Returns the value from the fseek() C function call.
 * [Input-Output]
 */
/** integer = fseek(fd, offset, whence); */
static mpdm_t F_fseek(F_ARGS)
{
    return MPDM_I(mpdm_fseek(A0, IA1, IA2));
}

/**
 * ftell - Returns the current file pointer.
 * @fd: the file descriptor
 *
 * Returns the position of the file pointer in @fd.
 * [Input-Output]
 */
/** integer = ftell(fd); */
static mpdm_t F_ftell(F_ARGS)
{
    return MPDM_I(mpdm_ftell(A0));
}

/**
 * feof - Returns the EOF condition of a file pointer.
 * @fd: the file descriptor
 *
 * Returns the EOF condition of @fd.
 * [Input-Output]
 */
/** bool = feof(fd); */
static mpdm_t F_feof(F_ARGS)
{
    return mpdm_bool(mpdm_feof(A0));
}

/**
 * flock - Locks a file.
 * @fd: the file descriptor
 * @operation: the operation
 *
 * Locks a file. The operation can be 1 (shared lock),
 * 2 (exclusive lock) or 4 (unlock). Closing the file
 * also unlocks. See the flock() system call man page.
 * This function does nothing under win32.
 *
 * Returns the value from the flock() C function call.
 * [Input-Output]
 */
/** integer = flock(fd, operation); */
static mpdm_t F_flock(F_ARGS)
{
    return MPDM_I(mpdm_flock(A0, IA1));
}

/**
 * unlink - Deletes a file.
 * @filename: file name to be deleted
 *
 * Deletes a file.
 * [Input-Output]
 */
/** bool = unlink(filename); */
static mpdm_t F_unlink(F_ARGS)
{
    return MPDM_I(mpdm_unlink(A0));
}

/**
 * rename - Renames a file.
 * @oldpath: old path
 * @newpath: new path
 *
 * Renames a file.
 * [Input-Output]
 */
/** bool = rename(oldpath, newpath); */
static mpdm_t F_rename(F_ARGS)
{
    return MPDM_I(mpdm_rename(A0, A1));
}

/**
 * stat - Gives status from a file.
 * @filename: file name to get the status from
 *
 * Returns a 14 element array of the status (permissions, onwer, etc.)
 * from the desired @filename, or NULL if the file cannot be accessed.
 * (man 2 stat).
 *
 * The values are: 0, device number of filesystem; 1, inode number;
 * 2, file mode; 3, number of hard links to the file; 4, uid; 5, gid;
 * 6, device identifier; 7, total size of file in bytes; 8, atime;
 * 9, mtime; 10, ctime; 11, preferred block size for system I/O;
 * 12, number of blocks allocated and 13, canonicalized file name.
 * Not all elements have necesarily meaningful values, as most are
 * system-dependent.
 * [Input-Output]
 */
/** array = stat(filename); */
static mpdm_t F_stat(F_ARGS)
{
    return mpdm_stat(A0);
}

/**
 * chmod - Changes a file's permissions.
 * @filename: the file name
 * @perms: permissions (element 2 from stat())
 *
 * Changes the permissions for a file.
 * [Input-Output]
 */
/** integer = chmod(filename, perms); */
static mpdm_t F_chmod(F_ARGS)
{
    return MPDM_I(mpdm_chmod(A0, A1));
}

/**
 * chown - Changes a file's owner.
 * @filename: the file name
 * @uid: user id (element 4 from stat())
 * @gid: group id (element 5 from stat())
 *
 * Changes the owner and group id's for a file.
 * [Input-Output]
 */
/** integer = chown(filename, uid, gid); */
static mpdm_t F_chown(F_ARGS)
{
    return MPDM_I(mpdm_chown(A0, A1, A2));
}

/**
 * glob - Executes a file globbing.
 * @spec: Globbing spec
 * @base: Optional base directory
 *
 * Executes a file globbing. @spec is system-dependent, but usually
 * the * and ? metacharacters work everywhere. @base can contain a
 * directory; if that's the case, the output strings will include it.
 * In any case, each returned value will be suitable for a call to
 * open().
 *
 * Returns an array of files that match the globbing (can be an empty
 * array if no file matches), or NULL if globbing is unsupported.
 * Directories are returned first and their names end with a slash.
 * [Input-Output]
 */
/** array = glob(spec, base); */
static mpdm_t F_glob(F_ARGS)
{
    return mpdm_glob(A0, A1);
}

/**
 * encoding - Sets the current charset encoding for files.
 * @charset: the charset name.
 *
 * Sets the current charset encoding for files. Future opened
 * files will be assumed to be encoded with @charset, which can
 * be any of the supported charset names (utf-8, iso-8859-1, etc.),
 * and converted on each read / write. If charset is NULL, it
 * is reverted to default charset conversion (i.e. the one defined
 * in the locale).
 *
 * This function stores the @charset value into the `ENCODING' global
 * variable.
 *
 * Returns a negative number if @charset is unsupported, or zero
 * if no errors were found.
 * [Input-Output]
 * [Character Set Conversion]
 */
/** integer = encoding(charset); */
static mpdm_t F_encoding(F_ARGS)
{
    return MPDM_I(mpdm_encoding(A0));
}

/**
 * popen - Opens a pipe.
 * @prg: the program to pipe
 * @mode: an fopen-like mode string
 *
 * Opens a pipe to a program. If @prg can be open in the specified @mode,
 * return file descriptor, or NULL otherwise.
 *
 * The @mode can be `r' (for reading), `w' (for writing), or `r+' or `w+'
 * for a special double pipe reading-writing mode.
 * [Input-Output]
 */
/** fd = popen(prg, mode); */
static mpdm_t F_popen(F_ARGS)
{
    return mpdm_popen(A0, A1);
}

/**
 * popen2 - Opens a pipe and returns an array of two pipes.
 * @prg: the program to pipe
 *
 * Opens a read-write pipe and returns an array of two descriptors,
 * one for reading and one for writing. If @prg could not be piped to,
 * returns NULL.
 * [Input-Output]
 */
/** array = popen2(prg); */
static mpdm_t F_popen2(F_ARGS)
{
    return mpdm_popen2(A0);
}

static mpdm_t F_regcomp(F_ARGS)
{
    return mpdm_regcomp(A0);
}

/**
 * regex - Matches a regular expression.
 * @v: the value to be matched
 * @r: the regular expression
 * @ra: an array of regular expressions
 * @offset: offset from the start of the value
 *
 * Matches a regular expression against a value. Valid flags are `i',
 * for case-insensitive matching, `m', to treat the string as a
 * multiline string (i.e., one containing newline characters), so
 * that ^ and $ match the boundaries of each line instead of the
 * whole string, `l', to return the last matching instead of the
 * first one, or `g', to match globally; in that last case, an array
 * containing all matches is returned instead of a string scalar.
 *
 * If @r is a string, an ordinary regular expression matching is tried
 * over the @v string. If the matching is possible, the match result
 * is returned, or NULL otherwise.
 *
 * If @r is an array (of strings), each element is tried sequentially
 * as an individual regular expression over the @v string, each one using
 * the offset returned by the previous match. All regular expressions
 * must match to be successful. If this is the case, an array (with
 * the same number of arguments) is returned containing the matched
 * strings, or NULL otherwise.
 *
 * If @r is NULL, the result of the previous regex matching
 * is returned as a two element array. The first element will contain
 * the character offset of the matching and the second the number of
 * characters matched. If the previous regex was unsuccessful, NULL
 * is returned.
 * [Regular Expressions]
 */
/** string = regex(v, r); */
/** string = regex(v, r, offset); */
/** array = regex(v, ra); */
/** array = regex(); */
static mpdm_t F_regex(F_ARGS)
{
    return mpdm_regex(A0, A1, IA2);
}

/**
 * sregex - Matches and substitutes a regular expression.
 * @v: the value to be matched
 * @r: the regular expression
 * @s: the substitution string, hash or code
 * @offset: offset from the start of v
 *
 * Matches a regular expression against a value, and substitutes the
 * found substring with @s. Valid flags are `i', for case-insensitive
 * matching, and `g', for global replacements (all ocurrences in @v
 * will be replaced, instead of just the first found one).
 *
 * If @s is executable, it's executed with the matched part as
 * the only argument and its return value is used as the
 * substitution string.
 *
 * If @s is a hash, the matched string is used as a key to it and
 * its value used as the substitution. If this value itself is
 * executable, it's executed with the matched string as its only
 * argument and its return value used as the substitution.
 *
 * If @r is NULL, returns the number of substitutions made in the
 * previous call to sregex() (can be zero if none was done).
 *
 * Returns the modified string, or the original one if no substitutions
 * were done.
 * [Regular Expressions]
 */
/** string = sregex(v, r, s); */
/** string = sregex(v, r, s, offset); */
/** integer = sregex(); */
static mpdm_t F_sregex(F_ARGS)
{
    return mpdm_sregex(A0, A1, A2, IA3);
}

/**
 * gettext - Translates a string to the current language.
 * @str: the string
 *
 * Translates the @str string to the current language.
 *
 * This function can still be used even if there is no real gettext
 * support by manually filling the __I18N__ hash.
 *
 * If the string is found in the current table, the translation is
 * returned; otherwise, the same @str value is returned.
 * [Strings]
 * [Localization]
 */
/** string = gettext(str); */
static mpdm_t F_gettext(F_ARGS)
{
    return mpdm_gettext(A0);
}

/**
 * gettext_domain - Sets domain and data directory for translations.
 * @dom: the domain (application name)
 * @data: directory contaning the .mo files
 *
 * Sets the domain (application name) and translation data for translating
 * strings that will be returned by gettext(). @data must point to a
 * directory containing the .mo (compiled .po) files.
 *
 * If there is no gettext support, returns 0, or 1 otherwise.
 * [Strings]
 * [Localization]
 */
/** bool = gettext_domain(dom, data); */
static mpdm_t F_gettext_domain(F_ARGS)
{
    return MPDM_I(mpdm_gettext_domain(A0, A1));
}

/**
 * load - Loads an MPSL source code file.
 * @source_file: the source code file
 *
 * Loads and executes an MPSL source code file and returns
 * its value.
 * [Code Control]
 */
/** load(source_file); */
static mpdm_t F_load(F_ARGS)
{
    return mpdm_exec(mpsl_compile_file(A0,
                                       mpsl_get_symbol(MPDM_S(L"INC"),
                                                       l)), NULL, l);
}

static mpdm_t F_resource(F_ARGS)
{
    return mpsl_resource(A0, mpsl_get_symbol(MPDM_S(L"INC"), l));
}

/**
 * compile - Compiles a string of MSPL source code file.
 * @source: the source code string
 *
 * Compiles a string of MPSL source code and returns an
 * executable value.
 * [Code Control]
 */
/** func = compile(code, source); */
static mpdm_t F_compile(F_ARGS)
{
    return mpsl_compile(A0, A1);
}


static mpdm_t F_decompile(F_ARGS)
{
    return mpsl_decompile(A0);
}


/**
 * error - Simulates an error.
 * @err: the error message
 *
 * Simulates an error. The @err error message is stored in the `ERROR'
 * global variable and an internal abort global flag is set, so no further
 * MPSL code can be executed until reset.
 * [Code Control]
 */
/** error(err); */
static mpdm_t F_error(F_ARGS)
{
    return mpsl_error(A0);
}

/**
 * uc - Converts a string to uppercase.
 * @str: the string to be converted
 *
 * Returns @str converted to uppercase.
 * [Strings]
 */
/** string = uc(str); */
static mpdm_t F_uc(F_ARGS)
{
    return mpdm_ulc(A0, 1);
}

/**
 * lc - Converts a string to lowercase.
 * @str: the string to be converted
 *
 * Returns @str converted to lowercase.
 * [Strings]
 */
/** string = uc(str); */
static mpdm_t F_lc(F_ARGS)
{
    return mpdm_ulc(A0, 0);
}

/**
 * time - Returns the current time.
 *
 * Returns the current time from the epoch, with decimals.
 * [Time]
 */
/** real = time(); */
static mpdm_t F_time(F_ARGS)
{
    return MPDM_R(mpdm_time());
}

/**
 * chdir - Changes the working directory
 * @dir: the new path
 *
 * Changes the working directory.
 * [Input-Output]
 */
/** integer = chdir(dir); */
static mpdm_t F_chdir(F_ARGS)
{
    return MPDM_I(mpdm_chdir(A0));
}

/**
 * mkdir - Creates a directory
 * @dir: the new path
 * @mode: permissions (number)
 *
 * Creates a directory.
 * [Input-Output]
 */
/** integer = mkdir(dir, mode); */
static mpdm_t F_mkdir(F_ARGS)
{
    return MPDM_I(mpdm_mkdir(A0, A1));
}

/**
 * getcwd - Returns the current working directory
 *
 * Returns the current working directory.
 * [Input-Output]
 */
/** path = getcwd(); */
static mpdm_t F_getcwd(F_ARGS)
{
    return mpdm_getcwd();
}

/**
 * sscanf - Extracts data like sscanf().
 * @str: the string to be parsed
 * @fmt: the string format
 * @offset: the character offset to start scanning
 *
 * Extracts data from a string using a special format pattern, very
 * much like the scanf() series of functions in the C library. Apart
 * from the standard percent-sign-commands (s, u, d, i, f, x,
 * n, [, with optional size and * to ignore), it implements S,
 * to match a string of characters upto what follows in the format
 * string. Also, the [ set of characters can include other % formats.
 *
 * Returns an array with the extracted values. If %n is used, the
 * position in the scanned string is returned as the value.
 * [Strings]
 */
/** array = sscanf(str, fmt); */
/** array = sscanf(str, fmt, offset); */
static mpdm_t F_sscanf(F_ARGS)
{
    return mpdm_sscanf(A0, A1, IA2);
}

/**
 * eval - Evaluates MSPL code.
 * @code: A value containing a string of MPSL code, or executable code
 * @args: optional arguments for @code
 *
 * Evaluates a piece of code. The @code can be a string containing MPSL
 * source code (that will be compiled) or a direct executable value. If
 * the compilation or the execution gives an error, the `ERROR' variable
 * will be set to a printable value and NULL returned. Otherwise, the
 * exit value from the code is returned and `ERROR' set to NULL. The 
 * internal abort flag is reset on exit.
 *
 * [Code Control]
 */
/** v = eval(code, args); */
static mpdm_t F_eval(F_ARGS)
{
    mpdm_t r, c;

    a = mpdm_ref(mpdm_clone(a));
    c = mpdm_shift(a);

    r = mpsl_eval(c, a, l);

    mpdm_unref(a);

    return r;
}


/**
 * sprintf - Formats a sprintf()-like string.
 * @fmt: the string format
 * @arg1: first argument
 * @arg2: second argument
 * @argn: nth argument
 *
 * Formats a string using the sprintf() format taking the values from
 * the variable arguments.
 * [Strings]
 */
/** string = sprintf(fmt, arg1 [,arg2 ... argn]); */
static mpdm_t F_sprintf(F_ARGS)
{
    mpdm_t f, v, r;

    a = mpdm_ref(mpdm_clone(a));
    f = mpdm_shift(a);

    /* if the first argument is an array, take it as the arguments */
    if ((v = mpdm_get_i(a, 0)) != NULL && mpdm_type(v) == MPDM_TYPE_ARRAY)
        a = v;

    r = mpdm_sprintf(f, a);

    mpdm_unref(a);

    return r;
}


/**
 * print - Writes values to stdout.
 * @arg1: first argument
 * @arg2: second argument
 * @argn: nth argument
 *
 * Writes the variable arguments to stdout.
 * [Input-Output]
 */
/** print(arg1 [,arg2 ... argn]); */
static mpdm_t F_print(F_ARGS)
{
    int n;

    for (n = 0; n < mpdm_size(a); n++) {
        if (n)
            mpdm_write_wcs(stdout, L" ");

        mpdm_write_wcs(stdout, mpdm_string(A(n)));
    }

    mpdm_write_wcs(stdout, L"\n");

    return NULL;
}


/**
 * write - Writes values to a file descriptor.
 * @fd: the file descriptor
 * @arg1: first argument
 * @arg2: second argument
 * @argn: nth argument
 *
 * Writes the variable arguments to the file descriptor, doing
 * charset conversion in the process.
 *
 * Returns @fd (versions previous to 2.2.5 returned the number
 * of bytes written).
 * [Input-Output]
 * [Character Set Conversion]
 */
/** integer = write(fd, arg1 [,arg2 ... argn]); */
static mpdm_t F_write(F_ARGS)
{
    mpdm_t f = A0;
    int n;

    for (n = 1; n < mpdm_size(a); n++)
        mpdm_write(f, A(n));

    return f;
}


/**
 * chr - Returns the Unicode character represented by the codepoint.
 * @c: the codepoint as an integer value
 *
 * Returns a 1 character string containing the character which
 * Unicode codepoint is @c.
 * [Strings]
 */
/** string = chr(c); */
static mpdm_t F_chr(F_ARGS)
{
    wchar_t tmp[2];

    tmp[0] = (wchar_t) mpdm_ival(mpdm_get_i(a, 0));
    tmp[1] = L'\0';

    return MPDM_S(tmp);
}


/**
 * ord - Returns the Unicode codepoint of a character.
 * @str: the string
 *
 * Returns the Unicode codepoint for the first character in
 * the string.
 * [Strings]
 */
/** integer = ord(str); */
static mpdm_t F_ord(F_ARGS)
{
    int ret = 0;
    mpdm_t v = mpdm_get_i(a, 0);

    if (v != NULL) {
        wchar_t *ptr = mpdm_string(v);
        ret = (int) *ptr;
    }

    return MPDM_I(ret);
}


/**
 * map - Maps a set to an array.
 * @set: the set
 * @filter: the filter
 *
 * Returns a new array built by applying the @filter by iterating
 * the @set.
 *
 * The set is iterated and can be an array, a hash, a scalar
 * (taken as an integer) or a file. If @filter is defined,
 * it's applied to the value and the index, and the result shall
 * be the new element for the array in this position.
 *
 * The filter can be an executable function accepting two arguments,
 * value and index, in which case the return value of the function
 * will be used as the output element; @filter can also be a hash,
 * in which case each array or hash value will be used as a
 * key to the hash and the associated value used as the output
 * element; also, it can be a string, which is then assumed to
 * be a regular expression that shall be matched against each value.
 *
 * [Arrays]
 * [Hashes]
 */
/** array = map(set, sub (v, i) { expression; }); */
/** array = map(set, hash); */
/** array = map(set, regex); */
static mpdm_t F_map(F_ARGS)
{
    return mpdm_map(A0, A1, l);
}


/**
 * omap - Maps a multiple value to a hash.
 * @set: the set (array or hash)
 * @filter: the filter
 *
 * Returns a new hash built by applying the @filter to all the elements
 * of the @set value. The filter can be an executable function
 * accepting one argument if @a is an array and two if @a is a hash,
 * in which case the return value of the function is expected to be a
 * two element array, with the 0th element to be the key and the 1st
 * element the value for the new pair in the output hash.
 *
 * If @filter is NULL, the returned hash is the inverse (values are
 * the new keys and keys are the new values).
 *
 * [Arrays]
 * [Hashes]
 */
/** hash = omap(set, filter); */
static mpdm_t F_omap(F_ARGS)
{
    return mpdm_omap(A0, A1, l);
}


/**
 * grep - Greps inside a multiple value.
 * @set: the array or hash
 * @filter: the filter
 *
 * Greps inside a multiple value and returns another one containing only the
 * elements that passed the filter. If @filter is a string, it's accepted
 * as a regular expression, which will be applied to each element or hash
 * key. If @filter is executable, it will be called with the element as
 * its only argument if @a is an array or with two if @a is a hash,
 * and its return value used as validation.
 *
 * The new value will have the same type as @a and will contain all
 * elements that passed the filter.
 * [Arrays]
 * [Regular Expressions]
 */
/** array = grep(set, filter); */
static mpdm_t F_grep(F_ARGS)
{
    return mpdm_grep(A0, A1, l);
}

static mpdm_t F_type(F_ARGS)
{
    return MPDM_S(mpdm_type_wcs(A0));
}


static mpdm_t F_getenv(F_ARGS)
{
    mpdm_t e = mpdm_get_wcs(mpdm_root(), L"ENV");

    return mpdm_get(e, mpdm_get_i(a, 0));
}

static mpdm_t F_bincall(F_ARGS)
{
    void *func;
    char *ptr = mpdm_wcstombs(mpdm_string(mpdm_get_i(a, 0)), NULL);

    sscanf(ptr, "%p", &func);
    free(ptr);

    return MPDM_X(func);
}


static mpdm_t F_valueptr(F_ARGS)
{
    void *val;
    char *ptr = mpdm_wcstombs(mpdm_string(mpdm_get_i(a, 0)), NULL);

    sscanf(ptr, "%p", &val);
    free(ptr);

    return val;
}


/**
 * random - Returns a random value.
 * @range: range of values.
 *
 * Returns a random value. If @range is an array, it returns
 * one of each elements randomly. If @range it's a integer,
 * returns a random integer from 0 to @range - 1. If @range
 * is 0, NULL or undefined, it returns a real number from
 * 0 to 1 (not included).
 * [Miscellaneous]
 */
/** integer = random(range); */
/** real = random(); */
/** element = random(array); */
static mpdm_t F_random(F_ARGS)
{
    return mpdm_random(A0);
};


/**
 * sleep - Sleeps a number of milliseconds.
 *
 * Sleeps a number of milliseconds.
 * [Threading]
 * [Time]
 */
/** sleep(msecs); */
static mpdm_t F_sleep(F_ARGS)
{
    mpdm_sleep(mpdm_ival(mpdm_get_i(a, 0)));

    return NULL;
}


/**
 * mutex - Returns a new mutex.
 *
 * Returns a new mutex.
 * [Threading]
 */
/** var = mutex(); */
static mpdm_t F_mutex(F_ARGS)
{
    return mpdm_new_mutex();
}


/**
 * mutex_lock - Locks a mutex (possibly waiting).
 * @mtx: the mutex
 *
 * Locks a mutex. If the mutex is already locked by
 * another process, it waits until it's unlocked.
 * [Threading]
 */
/** mutex_lock(mtx); */
static mpdm_t F_mutex_lock(F_ARGS)
{
    mpdm_mutex_lock(A0);
    return NULL;
}


/**
 * mutex_unlock - Unlocks a mutex.
 * @mtx: the mutex
 *
 * Unlocks a mutex.
 * [Threading]
 */
/** mutex_unlock(mtx); */
static mpdm_t F_mutex_unlock(F_ARGS)
{
    mpdm_mutex_unlock(A0);
    return NULL;
}


/**
 * semaphore - Returns a new semaphore.
 * cnt: the initial count of the semaphore.
 *
 * Returns a new semaphore.
 * [Threading]
 */
/** var = semaphore(cnt); */
static mpdm_t F_semaphore(F_ARGS)
{
    return mpdm_new_semaphore(IA0);
}


/**
 * semaphore_wait - Waits for a semaphore to be ready.
 * @sem: the semaphore to wait onto
 *
 * Waits for the value of a semaphore to be > 0. If it's
 * not, the thread waits until it is.
 * [Threading]
 */
/** semaphore_wait(sem); */
static mpdm_t F_semaphore_wait(F_ARGS)
{
    mpdm_semaphore_wait(A0);
    return NULL;
}


/**
 * semaphore_post - Increments the value of a semaphore.
 * @sem: the semaphore to increment
 *
 * Increments by 1 the value of a semaphore.
 * [Threading]
 */
/** semaphore_post(mtx); */
static mpdm_t F_semaphore_post(F_ARGS)
{
    mpdm_semaphore_post(A0);
    return NULL;
}


/**
 * tr - Transliterates a string.
 * @str: the string
 * @from: set of characters to be replaced
 * @to: set of characters to replace
 *
 * Transliterates @str to a new string with all characters from @from
 * replaced by those in @to.
 * [Threading]
 */
/** tr(str, from, to); */
static mpdm_t F_tr(F_ARGS)
{
    return mpdm_tr(A0, A1, A2);
}


/**
 * fmt - Formats one sprintf-like value.
 * @f: format string
 * @v: value
 *
 * Like sprintf(), but for only one value. It's a forward implementation
 * from MPSL 3.x $ (format operator). Accepts sprintf() percent-formats
 * plus j (JSON output) and t (brace-enclosed strftime() formats).
 * [Strings]
 */
/** string = fmt(@f, @v); */
static mpdm_t F_fmt(F_ARGS)
{
    return mpdm_fmt(A0, A1);
}


/**
 * connect - Opens a client TCP/IP socket.
 * @h: host name or ip
 * @s: service or port number
 *
 * Opens a client TCP/IP socket to the @h host at @s service (or port).
 * Returns NULL if the connection cannot be done or a file type value,
 * that can be used with all file operation functions, including close().
 * [Sockets]
 * [Input-Output]
 */
static mpdm_t F_connect(F_ARGS)
{
    return mpdm_connect(A0, A1);
}


/**
 * socket.server - Opens a server TCP/IP socket.
 * @addr: bind address (NULL for any)
 * @port: port number
 *
 * Opens a server TCP/IP socket in the specified @port number.
 * The @addr can be NULL to bind to all interfaces
 * or a hostname or IP to bind to a specific one (e.g. localhost).
 * [Sockets]
 * [Input-Output]
 */
static mpdm_t F_server(F_ARGS)
{
    return mpdm_server(A0, A1);
}


/**
 * socket.accept - Accepts a connection from a server socket.
 * @sock: the server socket
 *
 * Accepts a connection from the @sock server socket, that was created
 * in a call to @server.
 * [Sockets]
 * [Input-Output]
 */
static mpdm_t F_accept(F_ARGS)
{
    return mpdm_accept(A0);
}


/**
 * socket.timeout - Sets a socket's receiving and/or sending timeouts.
 * @sock: the socket
 * @r_to: receiving timeout (in seconds)
 * @s_to: sending timeout (in seconds)
 *
 * Sets the receiving and/or sending timeouts (in seconds) for @sock.
 * If any of the timeouts is NULL, it's not touched.
 * [Sockets]
 * [Input-Output]
 */
static mpdm_t F_socket_timeout(F_ARGS)
{
    return mpdm_socket_timeout(A0, A1, A2);
}



/**
 * new - Creates a new object using another as its base.
 * @c1: class / base object
 * @c2: class / base object
 * @cn: class / base object
 *
 * Creates a new object using as classes or base objects all the ones
 * sent as arguments (assumed to be hashes).
 *
 * If the new object has an __init__() method, it's executed with
 * the newly created object as its unique argument. It MUST return
 * the same object or NULL if the object has been destroyed due to
 * a failed initialization.
 * 
 * [Object-oriented programming]
 */
/** o = new(c1 [, c2, ...cn]); */
static mpdm_t F_new(F_ARGS)
{
    int n;
    mpdm_t i, r = MPDM_O();

    for (n = 0; n < mpdm_size(a); n++) {
        mpdm_t w, v, i;
        int m = 0;

        w = mpdm_ref(A(n));

        if (mpdm_type(w) == MPDM_TYPE_OBJECT) {
            while (mpdm_iterator(w, &m, &v, &i))
                mpdm_set(r, mpdm_clone(v), i);
        }

        mpdm_unref(w);
    }

    /* if it has an __init__ executable value, run it */
    if ((i = mpdm_get_wcs(r, L"__init__")) && mpdm_can_exec(i)) {
        mpdm_t nr;

        /* call it, referencing the object */
        nr = mpdm_ref(mpdm_exec_1(i, mpdm_ref(r), NULL));

        /* unref the original object */
        mpdm_unref(r);

        /* unref the returned object without deleting */
        r = mpdm_unrefnd(nr);
    }

    return r;
}


static mpdm_t F_get(F_ARGS)
{
    return mpdm_get(A0, A1);
}


static mpdm_t F_del(F_ARGS)
{
    return mpdm_del(A0, A1);
}


static mpdm_t F_set(F_ARGS)
{
    return mpdm_set(A0, A1, A2);
}


static mpdm_t F_eol(F_ARGS)
{
    return MPDM_S(mpdm_eol(A0));
}


static mpdm_t F_integer(F_ARGS)
{
    return MPDM_I(mpdm_ival(A0));
}


static mpdm_t F_real(F_ARGS)
{
    return MPDM_R(mpdm_rval(A0));
}


static mpdm_t F_string(F_ARGS)
{
    return MPDM_S(mpdm_string(A0));
}


static mpdm_t F_count(F_ARGS)
{
    return MPDM_I(mpdm_count(A0));
}


static mpdm_t F_pow(F_ARGS)
{
    return MPDM_R(pow(RA0, RA1));
}


static mpdm_t F_bitand(F_ARGS)
{
    return MPDM_I(IA0 & IA1);
}


static mpdm_t F_bitor(F_ARGS)
{
    return MPDM_I(IA0 | IA1);
}


static mpdm_t F_bitxor(F_ARGS)
{
    return MPDM_I(IA0 ^ IA1);
}


static mpdm_t F_bitshr(F_ARGS)
{
    return MPDM_I(IA0 >> IA1);
}


static mpdm_t F_bitshl(F_ARGS)
{
    return MPDM_I(IA0 << IA1);
}


static mpdm_t F_chomp(F_ARGS)
{
    return mpdm_chomp(A0);
}


static mpdm_t F_index(F_ARGS)
{
    return A1;
}


static mpdm_t F_swap(F_ARGS)
{
    mpdm_t r = MPDM_A(2);

    mpdm_set_i(r, A1, 0);
    mpdm_set_i(r, A0, 1);

    return r;
}


static mpdm_t F_md5(F_ARGS)
{
    return mpdm_md5(A0);
}


static mpdm_t F_base64enc(F_ARGS)
{
    return mpdm_base64enc(A0);
}


static mpdm_t F_unicode_nfd(F_ARGS)
{
    return mpdm_unicode_nfd(A0);
}


static mpdm_t F_unicode_nfc(F_ARGS)
{
    return mpdm_unicode_nfc(A0);
}


static struct {
    wchar_t *name;
     mpdm_t(*func) (mpdm_t, mpdm_t);
} mpsl_funcs[] = {
    { L"size",           F_size },
    { L"clone",          F_clone },
    { L"dump",           F_dump },
    { L"dumper",         F_dumper },
    { L"cmp",            F_cmp },
    { L"is_exec",        F_is_exec },
    { L"splice",         F_splice },
    { L"expand",         F_expand },
    { L"collapse",       F_collapse },
    { L"ins",            F_ins },
    { L"shift",          F_shift },
    { L"push",           F_push },
    { L"pop",            F_pop },
    { L"queue",          F_queue },
    { L"seek",           F_seek },
    { L"sort",           F_sort },
    { L"split",          F_split },
    { L"join",           F_join },
    { L"exists",         F_exists },
    { L"open",           F_open },
    { L"close",          F_close },
    { L"read",           F_read },
    { L"write",          F_write },
    { L"getchar",        F_getchar },
    { L"putchar",        F_putchar },
    { L"fseek",          F_fseek },
    { L"ftell",          F_ftell },
    { L"feof",           F_feof },
    { L"flock",          F_flock },
    { L"unlink",         F_unlink },
    { L"rename",         F_rename },
    { L"stat",           F_stat },
    { L"chmod",          F_chmod },
    { L"chown",          F_chown },
    { L"glob",           F_glob },
    { L"encoding",       F_encoding },
    { L"popen",          F_popen },
    { L"popen2",         F_popen2 },
    { L"pclose",         F_close },
    { L"regcomp",        F_regcomp },
    { L"regex",          F_regex },
    { L"sregex",         F_sregex },
    { L"load",           F_load },
    { L"compile",        F_compile },
    { L"decompile",      F_decompile },
    { L"resource",       F_resource },
    { L"error",          F_error },
    { L"eval",           F_eval },
    { L"print",          F_print },
    { L"gettext",        F_gettext },
    { L"gettext_domain", F_gettext_domain },
    { L"sprintf",        F_sprintf },
    { L"chr",            F_chr },
    { L"ord",            F_ord },
    { L"map",            F_map },
    { L"omap",           F_omap },
    { L"grep",           F_grep },
    { L"getenv",         F_getenv },
    { L"uc",             F_uc },
    { L"lc",             F_lc },
    { L"time",           F_time },
    { L"chdir",          F_chdir },
    { L"mkdir",          F_mkdir },
    { L"getcwd",         F_getcwd },
    { L"sscanf",         F_sscanf },
    { L"bincall",        F_bincall },
    { L"valueptr",       F_valueptr },
    { L"random",         F_random },
    { L"sleep",          F_sleep },
    { L"mutex.new",      F_mutex },
    { L"mutex.lock",     F_mutex_lock },
    { L"mutex.unlock",   F_mutex_unlock },
    { L"semaphore.new",  F_semaphore },
    { L"semaphore.wait", F_semaphore_wait },
    { L"semaphore.post", F_semaphore_post },
    { L"tr",             F_tr },
    { L"socket.connect", F_connect },
    { L"socket.server",  F_server },
    { L"socket.accept",  F_accept },
    { L"socket.timeout", F_socket_timeout },
    { L"new",            F_new },
    { L"fmt",            F_fmt },
    { L"slice",          F_slice },
    { L"reverse",        F_reverse },
    { L"bool",           F_bool },
    { L"type",           F_type },
    { L"get",            F_get },
    { L"del",            F_del },
    { L"set",            F_set },
    { L"eol",            F_eol },
    { L"integer",        F_integer },
    { L"real",           F_real },
    { L"string",         F_string },
    { L"count",          F_count },
    { L"pow",            F_pow },
    { L"bitand",         F_bitand },
    { L"bitor",          F_bitor },
    { L"bitxor",         F_bitxor },
    { L"bitshl",         F_bitshl },
    { L"bitshr",         F_bitshr },
    { L"escape",         F_escape },
    { L"chomp",          F_chomp },
    { L"index",          F_index },
    { L"swap",           F_swap },
    { L"md5",            F_md5 },
    { L"base64.enc",     F_base64enc },
    { L"unicode.nfd",    F_unicode_nfd },
    { L"unicode.nfc",    F_unicode_nfc },
    { NULL,              NULL }
};


mpdm_t mpsl_build_funcs(void)
/* build all functions */
{
    mpdm_t c;
    int n;

    /* creates all the symbols in the CORE library */
    c = MPDM_O();

    for (n = 0; mpsl_funcs[n].name != NULL; n++) {
        mpdm_t f = MPDM_S(mpsl_funcs[n].name);
        mpdm_t x = MPDM_X(mpsl_funcs[n].func);

        /* does the function name have dots? */
        if (wcschr(mpsl_funcs[n].name, L'.')) {
            mpdm_t p = mpdm_ref(mpdm_split(f, MPDM_S(L".")));
            mpdm_t m, o;

            /* search for the base object */
            m = mpdm_pop(p);
            f = mpdm_ref(mpdm_pop(p));

            if ((o = mpdm_get(mpdm_root(), f)) == NULL)
                o = MPDM_O();

            mpdm_set(o, x, m);
            mpdm_set(mpdm_root(), o, f);
            mpdm_set(c, o, f);

            mpdm_unref(f);
            mpdm_unref(p);
        }
        else {
            mpdm_set(mpdm_root(), x, f);
            mpdm_set(c, x, f);
        }
    }

    return c;
}
