#!/usr/bin/env python3
import argparse
import contextlib
import pathlib
import shutil
import subprocess
import tempfile

SIZES = [16, 24, 32, 40, 48, 64, 96, 128, 256]


def run(cmd):
    subprocess.run(cmd, check=True)


def try_run(cmd):
    result = subprocess.run(cmd, text=True, capture_output=True)
    return result.returncode == 0, result.stderr.strip()


def magick_cmd():
    magick = shutil.which('magick')
    if not magick:
        raise RuntimeError('missing tool: install ImageMagick (magick)')
    return magick


@contextlib.contextmanager
def magick_svg(svg):
    text = pathlib.Path(svg).read_text(encoding='utf-8')
    fixed = text.replace('href="image/', 'href="data:image/')
    fixed = fixed.replace('xlink:href="image/', 'xlink:href="data:image/')
    if fixed == text:
        yield str(svg)
        return
    with tempfile.NamedTemporaryFile('w', suffix='.svg', delete=False, encoding='utf-8') as tmp:
        tmp.write(fixed)
        tmp_path = tmp.name
    try:
        yield tmp_path
    finally:
        pathlib.Path(tmp_path).unlink(missing_ok=True)


def render_png(svg, out_png, size):
    errors = []
    rsvg = shutil.which('rsvg-convert')
    if rsvg:
        ok, err = try_run([rsvg, '-w', str(size), '-h', str(size), '-o', str(out_png), str(svg)])
        if ok:
            return
        errors.append(err)
    inkscape = shutil.which('inkscape')
    if inkscape:
        ok, err = try_run([
            inkscape,
            str(svg),
            '--export-type=png',
            '--export-width',
            str(size),
            '--export-height',
            str(size),
            '--export-filename',
            str(out_png),
        ])
        if ok:
            return
        errors.append(err)
    with magick_svg(svg) as svg_for_magick:
        ok, err = try_run([
            magick_cmd(),
            svg_for_magick,
            '-background',
            'none',
            '-resize',
            f'{size}x{size}',
            str(out_png),
        ])
    if ok:
        return
    errors.append(err)
    raise RuntimeError('failed to render png: ' + ' | '.join(e for e in errors if e))


def build_ico(svg, out_ico):
    with magick_svg(svg) as svg_for_magick:
        run([
            magick_cmd(),
            svg_for_magick,
            '-background',
            'none',
            '-define',
            'icon:auto-resize=' + ','.join(str(size) for size in SIZES),
            str(out_ico),
        ])


def parse_args():
    parser = argparse.ArgumentParser(prog='generate_icons.py')
    parser.add_argument('input_svg')
    parser.add_argument('output_png')
    parser.add_argument('--ico', dest='output_ico')
    parser.add_argument('--size', type=int, default=48)
    return parser.parse_args()


def main():
    args = parse_args()
    magick_cmd()
    svg = pathlib.Path(args.input_svg)
    out_png = pathlib.Path(args.output_png)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    render_png(svg, out_png, args.size)
    if args.output_ico:
        out_ico = pathlib.Path(args.output_ico)
        out_ico.parent.mkdir(parents=True, exist_ok=True)
        build_ico(svg, out_ico)


if __name__ == '__main__':
    main()
