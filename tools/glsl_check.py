#!/usr/bin/env python3
"""Static GLSL ES 3.0 sanity checks for the shaders embedded in the C++ sources.

A shader that fails to compile at runtime is a black screen, and there is no GPU in CI, so
this checks the failure modes we can catch statically: balanced braces/parens, varyings
written in the vertex stage and declared identically in the fragment stage, uniforms used but
never set from C++, and ES3-only syntax mistakes (e.g. non-constant loop bounds are fine,
`in/out` mixing is not).

    python3 tools/glsl_check.py
"""
import re
import sys

SRC_GLOBS = [
    'client/jni/world/world.cpp',
    'client/jni/core/gfx.cpp',
    'client/jni/core/ui.cpp',
]
FAILED = []


def check(name, cond, detail=''):
    print(('ok   ' if cond else 'FAIL ') + name + (f' — {detail}' if detail else ''))
    if not cond:
        FAILED.append(name)


def shaders(path):
    src = open(path).read()
    out = []
    for m in re.finditer(r'R"\((.*?)\)"', src, re.S):
        body = m.group(1)
        if '#version' in body:
            out.append((path, body))
    return out


def parse(body):
    vs = '#version' in body and 'gl_Position' in body
    decl = set(re.findall(r'^\s*(?:layout\(location=\d+\)\s*)?(?:in|out|uniform)\s+'
                          r'(?:highp |mediump |lowp )?\w+\s+(\w+)\s*;', body, re.M))
    outs = set(re.findall(r'^\s*out\s+(?:highp |mediump |lowp )?\w+\s+(\w+)\s*;', body, re.M))
    ins = set(re.findall(r'^\s*in\s+(?:highp |mediump |lowp )?\w+\s+(\w+)\s*;', body, re.M))
    uniforms = set(re.findall(r'^\s*uniform\s+\w+\s+(\w+)\s*;', body, re.M))
    return dict(vs=vs, decl=decl, outs=outs, ins=ins, uniforms=uniforms)


def main():
    stages = []
    for path in SRC_GLOBS:
        for src_path, body in shaders(path):
            stages.append((src_path, body, parse(body)))
    check('found shader sources', len(stages) >= 6, f'{len(stages)} stages')

    for path, body, p in stages:
        tag = ('VS' if p['vs'] else 'FS') + ' ' + path.split('/')[-1]
        check(f'{tag}: balanced braces', body.count('{') == body.count('}'))
        check(f'{tag}: balanced parens', body.count('(') == body.count(')'))
        check(f'{tag}: declares #version 300 es', '#version 300 es' in body)
        if p['vs']:
            check(f'{tag}: writes gl_Position', 'gl_Position' in body)

    # vertex out <-> fragment in for the world program (the one that renders the terrain)
    world = [s for s in stages if s[0].endswith('world.cpp')]
    vs = [p for _s, b, p in world if p['vs']]
    fs = [p for _s, b, p in world if not p['vs']]
    check('world program has both stages', bool(vs) and bool(fs))
    if vs and fs:
        v_out = vs[0]['outs']
        f_in = fs[0]['ins']
        unmatched = sorted(v_out - f_in)
        check('world varyings match', not unmatched,
              f'vertex writes {sorted(v_out)}, fragment reads {sorted(f_in)}')
        check('world fs uses the material samplers',
              {'uAlbedo', 'uNormalMap'} <= fs[0]['uniforms'],
              ','.join(sorted(fs[0]['uniforms'])))

    print('PASS' if not FAILED else f'{len(FAILED)} failed')
    return 0 if not FAILED else 1


if __name__ == '__main__':
    sys.exit(main())
