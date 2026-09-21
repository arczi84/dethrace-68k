# BRender / PiStorm3D depth stress test

`brdepth` is a small AmigaOS diagnostic for the intermittent depth/occlusion
corruption seen in Dethrace on PiStorm3D. It deliberately uses the same path as
the game:

`BRender softrend front end -> BRender 3dfx driver -> Glide shim -> shared MiniGL`

The scene contains textured boxes with long triangles, steep depth gradients
and a large near/far range. Geometry is submitted in a mixed order. Correct Z
buffering must always keep nearer boxes in front of more distant boxes.

## Controls

- `Up`/`Down`: precise camera movement in 0.1-unit steps.
- `Left`/`Right`: fast camera movement in 2.0-unit steps.
- `5`: set camera Z to `-6.8`.
- `6`: set camera Z to `-6.7`.
- `1`: original Carmageddon/Glide Q path (`sow`, `tow`, `oow`). This is the
  important mode for reproducing the game bug.
- `2`: affine texture-coordinate reference.
- `3`: reconstructed vertex-W diagnostic.
- `4`: reconstructed eye-XYZ / `glFrustum` diagnostic.
- `Esc`: exit.

The program renders only after input, so it does not continuously consume the
CPU. Current camera distance and library information are written to
`brdepth.log`.

The camera can travel through the complete corridor, from Z=80 to Z=-75.
The initial camera position is Z=-6.8. The glitch boundary was observed around
Z=-6.8/-6.7, and keys `5` and `6` select these exact positions.
All geometry intentionally shares one texture, one material and one cube model.
Separating rows into different materials changes BRender/MiniGL batching and
suppresses the driver bug this program is intended to reproduce.

## Failure signature

While changing camera distance in mode 1, a distant building or long face may
sporadically appear through or on top of a nearer object. The same kind of
distance-dependent corruption is visible in Carmageddon, while simpler two-quad
depth tests pass.

The executable is linked with the v12 MiniGL dispatch import library and loads
the installed `minigl.library` at runtime.
