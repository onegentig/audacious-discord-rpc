# audacious-plugin-rpc

I just didn’t like the style as much, so I made a fork. I made the artist
and track title appear separately + added time elapsed.

<div align="center">
     <img
          src=".github/img/popout.png"
          alt="User popout" />
</div>

The changes without this `README.md` (if someone wanted to make a PR or smth) is in branch
[`refactor/style`](https://github.com/onegentig/audacious-discord-rpc/tree/refactor/style).

Don’t @ me about a missing licence,
the original repo doesn’t have one either. \
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
