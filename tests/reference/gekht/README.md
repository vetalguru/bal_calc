# Reference trajectories from BallisticCalculator1

Copied unchanged from
[gehtsoft-usa/BallisticCalculator1](https://github.com/gehtsoft-usa/BallisticCalculator1)
(`BallisticCalculator.Test/resources/`, commit `18d59fc756b4`, 2026-08-25),
Copyright Gehtsoft USA, LLC, licensed under the GNU LGPL 2.1 (which allows
use under the GPL, see its section 3). They are the reference tables that
project checks its own calculator against: `g*_*.txt` from a third-party 3-DOF
calculator, `be_coriolis_*.txt` from Ballistic Explorer.

Format (`;`-separated; units are written next to the numbers):

```
ammo;<BC><table>;<weight>;<muzzle velocity>[;<diameter>;<length>]
rifle;<sight height>;<zero distance>[;<twist>;right|left]
wind;<speed>;<direction>          0 = toward the target, 90 = from the right
atmosphere;<temperature>;<humidity %>;<pressure>;<altitude>
shot;<look angle>;<cant>[;<azimuth>;<latitude>]
<units of the columns>
<range>;<drop>;<drop angle>;<windage>;<windage angle>;<velocity>;<mach>;<energy>;<time>[;...]
```

Drop is up-positive from the line of sight; windage is **left**-positive
(the opposite of ours). The zero is found in the same air with no wind and no
Earth rotation. `tests/physics_gekht_test.cpp` reads them with the tolerances
that project uses.
