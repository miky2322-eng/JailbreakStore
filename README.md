# Jailbreak Store

PS5 payload that rebrands the PlayStation Store (`NPXS40047`) into a
custom launcher.

⚠️ **WARNING: Messing around with PS5's database can corrupt the database
and you will lose all your PS4 FPKGs.**

## What it does

Running `JailbreakStore.elf` on a jailbroken PS5:

1. Writes a custom icon to `/user/appmeta/NPXS40047/icon0.png`.
2. Renames the tile to "Jailbreak Store".
3. Redirects the tile's launch URL, so pressing it opens the PS5's webview
   at a chosen page instead of the real Store.

All changes are done with `UPDATE`/`INSERT` statements on existing rows.
Nothing is deleted, and the Store's own row is never removed.

## Build

Requires the [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk).

```
make                        # build JailbreakStore.elf
make clean                  # remove build output
PS5_PAYLOAD_SDK=/path make  # use a different SDK location
PS5_HOST=<ip> make deploy   # build + send to console
```

Edit `NEW_TITLE`, `NEW_DEEPLINK`, and `icon0.png` in `source/main.c` /
the project root before building to customize the name, URL, and icon.

## Changing the Icon

1. Replace `icon0.png` in the project root with your own image, already
   square (512x512 matches the console's own tile icons). The build
   embeds the file as-is -- it does not resize or pad it, so a non-square
   source will render stretched on the tile.
2. Run `make`. The Makefile regenerates `source/icon0_png.h` from
   `icon0.png` automatically, embedding it into the build.
3. Send the rebuilt `JailbreakStore.elf` and reboot. It writes the new
   icon to `/user/appmeta/NPXS40047/icon0.png` on the console.

Once `icon0Info` points there, you can also skip all of the above and
just overwrite `/user/appmeta/NPXS40047/icon0.png` directly (FTP or any
file manager) -- no rebuild, no resend, just reboot to see it.

## Run

Send `JailbreakStore.elf` to the console the same way you'd load any
other payload, then reboot to see the changes.
