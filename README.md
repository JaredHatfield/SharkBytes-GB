# Shark Bytes

A minimal Game Boy project scaffold built with [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020).

Right now the ROM only boots to a title screen that displays `Shark Bytes`.

## Build

Install the official GBDK SDK once:

```sh
just sdk-install
```

Then build the ROM:

```sh
just build
```

You can override the local SDK path with an existing installation:

```sh
GBDK_HOME=/path/to/gbdk just build
```

For quick iteration in SameBoy:

```sh
just sameboy
```

The ROM is written to `dist/shark-bytes.gb`.
