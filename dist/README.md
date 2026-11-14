# NEXORA 1.0.0 APK (split)

GitHub rejects files above 100 MB, and this APK is **108.6 MB** on purpose: the size is real
content (photoreal 1K/2K material atlases and nine composed 90–240 s music tracks), not padding.

Reassemble and verify:

```bash
cd client/build/split
cat nexora-1.0.0.apk.part-* > nexora-1.0.0.apk
md5sum -c nexora-1.0.0.apk.md5
```

| Field | Value |
| --- | --- |
| package | `com.nexora.game` 1.0.0 |
| ABI | arm64-v8a |
| minSdk / targetSdk | 26 / 34 |
| size | 108.58 MB (115,502,585 bytes at build time; see md5) |
| signing | v1 + v2 + v3, RSA-4096 |
| native lib | stored uncompressed, 16 KB page-aligned |
| soundtrack | 9 IMA-ADPCM streams (exploration, social, race, combat, horror, menu, victory, city, storm) |
| materials | terrain atlas (grass/sand/rock/snow) + prop atlas (brick/planks/cobble/metal), albedo + normal |

The APK is a build artifact: `tools/release.sh` regenerates it from the tree
(native library → repack → sign → `tools/check_apk.py`).
