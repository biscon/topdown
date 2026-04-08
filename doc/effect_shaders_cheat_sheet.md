# Adventure Engine – Effect Shader Presets Cheat Sheet

Quick starting values for common scene effects.
All values are meant as **tweakable baselines**, not strict rules.

---

## 🌊 Water Ripple (`water_ripple`)

**Use for:** lakes, ponds, shoreline water distortion

### Calm Lake

```
distortionX = 6.0
distortionY = 3.0
uvScaleX    = 1.2
uvScaleY    = 1.0
noiseSpeedX = 0.3
noiseSpeedY = 0.2
intensity   = 0.6
phaseOffset = 0.0
softness    = 0.2
```

### Windy / Choppy Water

```
distortionX = 10.0
distortionY = 6.0
uvScaleX    = 2.0
uvScaleY    = 1.6
noiseSpeedX = 0.8
noiseSpeedY = 0.6
intensity   = 1.0
phaseOffset = 0.0
softness    = 0.2
```

---

## 🌫 Heat Shimmer (`heat_shimmer`)

**Use for:** hot air, magical distortion, subtle background movement

### Subtle Atmosphere

```
distortionX = 4.0
distortionY = 6.0
uvScaleX    = 1.5
uvScaleY    = 2.0
noiseSpeedX = 0.6
noiseSpeedY = 0.8
intensity   = 0.5
phaseOffset = 0.0
softness    = 0.25
```

### Strong Heat / Magic Field

```
distortionX = 8.0
distortionY = 10.0
uvScaleX    = 2.5
uvScaleY    = 3.0
noiseSpeedX = 1.2
noiseSpeedY = 1.5
intensity   = 1.0
phaseOffset = 0.0
softness    = 0.2
```

---

## 🎨 Region Grade (`region_grade`)

**Use for:** lighting zones, mood changes, underwater tint, indoor/outdoor transitions

### Warm Light (sunset / lamp glow)

```
brightness = 0.05
contrast   = 1.1
saturation = 1.2
tintR      = 1.1
tintG      = 0.95
tintB      = 0.85
softness   = 0.3
```

### Cool / Moonlight

```
brightness = -0.02
contrast   = 1.05
saturation = 0.9
tintR      = 0.85
tintG      = 0.95
tintB      = 1.1
softness   = 0.35
```

### Underwater / Damp Area

```
brightness = -0.05
contrast   = 0.95
saturation = 0.85
tintR      = 0.7
tintG      = 0.9
tintB      = 1.1
softness   = 0.4
```

---

## 🌬 UV Scroll (`uv_scroll`)

**Use for:** fog layers, mist, shadow movement, subtle overlays

### Slow Fog Drift

```
scrollSpeedX = 0.01
scrollSpeedY = 0.0
uvScaleX     = 1.0
uvScaleY     = 1.0
phaseOffset  = 0.0
softness     = 0.3
```

### Moving Shadows (forest canopy)

```
scrollSpeedX = 0.03
scrollSpeedY = 0.01
uvScaleX     = 1.5
uvScaleY     = 1.5
phaseOffset  = 0.0
softness     = 0.25
```

### Dense Mist Layer

```
scrollSpeedX = 0.02
scrollSpeedY = -0.01
uvScaleX     = 2.0
uvScaleY     = 2.0
phaseOffset  = 0.0
softness     = 0.4
```

---

## 🌿 Wind Sway (`wind_sway`)

**Use for:** reeds, grass, vines, hanging foliage

### Subtle Breeze (trees / branches)

```
distortionX = 0.008
uvScaleX    = 1.0
uvScaleY    = 1.5
noiseSpeedX = 1.2
intensity   = 0.8
phaseOffset = 0.0
softness    = 0.2
```

### Light Wind (grass / reeds)

```
distortionX = 0.015
uvScaleX    = 1.5
uvScaleY    = 2.0
noiseSpeedX = 1.8
intensity   = 1.0
phaseOffset = 0.0
softness    = 0.2
```

### Strong Wind

```
distortionX = 0.025
uvScaleX    = 2.0
uvScaleY    = 2.5
noiseSpeedX = 2.5
intensity   = 1.2
phaseOffset = 0.0
softness    = 0.2
```

---

## ✂️ Poly Clip (`poly_clip`)

**Use for:** masking an image/overlay into a rectangle or polygon without distortion

---

### Soft Irregular Overlay

```
softness = 0.12
```

### Sharp Decal / Foam Edge

```
softness = 0.03
```

### Very Soft Mist / Light Patch

```
softness = 0.22
```

---

## 🧠 Notes

* Best for:

  * shoreline foam
  * puddles
  * wet ground patches
  * local mist
  * dappled light
  * moss / dirt overlays

* The texture is:

  * **fit to the region rect**
  * then **clipped by the polygon**

* Typical range:

```
softness = 0.05 → 0.20
```

* Works best with:

  * textures that already have alpha / soft edges
  * subtle overlays rather than hard graphic shapes

---

## 🧠 General Tips

* **Start subtle** — most good effects are barely noticeable
* **Layer effects**:

  * water = ripple + faint uv_scroll overlay
  * forest = uv_scroll shadows + wind_sway foreground
* **Use polygon regions** to avoid rectangular artifacts
* **Softness is your best friend** for blending effects
* **PhaseOffset** is great for avoiding synchronized movement

---

## 🔥 Quick Combo Ideas

### Lake Scene

* `water_ripple` (polygon masked)
* `uv_scroll` (faint blue noise overlay, additive)
* `region_grade` (slight cool tint near water)

### Forest Edge

* `uv_scroll` (multiply shadow movement)
* `wind_sway` (foreground reeds / grass)
* `region_grade` (slightly darker under trees)

### Magic Area

* `heat_shimmer`
* `region_grade` (color tint)
* optional `uv_scroll` noise overlay

---

