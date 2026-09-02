#!/usr/bin/env python3
# Embeds a PNG file as a C byte array header for the payload to write out on-device.
import sys

src, dst, symbol = sys.argv[1], sys.argv[2], sys.argv[3]
data = open(src, "rb").read()

with open(dst, "w") as f:
    f.write("/* Generated from %s. Do not edit by hand. */\n" % src)
    f.write("static const unsigned char g_%s[] = {\n" % symbol)
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        f.write("    " + ",".join(str(b) for b in chunk) + ",\n")
    f.write("};\n")
    f.write("static const unsigned int g_%s_len = %d;\n" % (symbol, len(data)))
