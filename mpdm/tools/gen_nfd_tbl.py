#!/usr/bin/python3

# Generates a table of Unicode NFD-NFC normalizations
# public domain - <dev@triptico.com>

import unicodedata

print("/* automatically generated */\n")

nfd_tbl = []

for c in range(128, 0x530):
    # create an NFD-normalized string for this character
    s = unicodedata.normalize("NFD", chr(c))

    if len(s) != 1:
        # NFC string
        nfc_s = "\\x%x" % c
        # convert the NFD string to hex
        nfd_s = "".join([ "\\x%x" % ord(pc) for pc in s ])

        nfd_tbl.append([nfc_s, nfd_s, len(s), unicodedata.name(chr(c)), s])

print("#define NFD_TBL_SIZE %d\n" % len(nfd_tbl))

print("struct unicode_nfd_tbl_e {")
print("    wchar_t nfc;")
print("    wchar_t *nfd;")
print("    int size;")
print("} unicode_nfd_tbl[NFD_TBL_SIZE] = {")

for e in nfd_tbl:
    print("    { L'%s', L\"%s\", %d }, /* %s */" % (e[0], e[1], e[2], e[3]))

print("};")

nfc_tbl = sorted(nfd_tbl, key=lambda nfd: nfd[4])

print("struct unicode_nfc_tbl_e {")
print("    wchar_t *nfd;")
print("    wchar_t nfc;")
print("    int size;")
print("} unicode_nfc_tbl[NFD_TBL_SIZE] = {")

for e in nfc_tbl:
    print("    { L\"%s\", L'%s', %d }, /* %s */" % (e[1], e[0], e[2], e[3]))

print("};")
