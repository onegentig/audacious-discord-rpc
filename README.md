# Audacious Discord RPC

Minor modernisation fork of [`darktohka/audacious-plugin-rpc`](https://github.com/darktohka/audacious-plugin-rpc)
with the artist and track displayed separately and a time elapsed counter.

<div align="center">
     <img
          src=".github/img/popout.png"
          alt="User popout" />
</div>

The changes without this `README.md` is in branch
[`refactor/style`](https://github.com/onegentig/audacious-discord-rpc/tree/refactor/style).

Do not @ me about the missing licence,
the original repo doesn’t have one either ([#15](https://github.com/darktohka/audacious-plugin-rpc/issues/15)). \
If you are the original author pls add one thx.

**Before:**

<img
     src=".github/img/before.png"
     alt="User popout"
     align="center"
     width="70%" />

**After:**

<img
     src=".github/img/after.png"
     alt="User popout"
     align="center"
     width="70%" />

## Installation

```sh
git clone git@github.com:onegentig/audacious-discord-rpc.git
cd audacious-discord-rpc
mkdir build
cd build
cmake ..
rm _deps/discord-rpc-src/.clang-format
sudo make install
```
