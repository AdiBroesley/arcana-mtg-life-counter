"""Export the local Face Studio gallery. Optional export dependency: Pillow."""
from pathlib import Path
import argparse
import html
import json
import os
import re
import subprocess
import textwrap
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def font(size, bold=False):
    path = Path('C:/Windows/Fonts') / ('seguisb.ttf' if bold else 'segoeui.ttf')
    return ImageFont.truetype(str(path), size) if path.exists() else ImageFont.load_default(size=size)


def export(directory, render=False):
    directory.mkdir(parents=True, exist_ok=True)
    if render:
        env = dict(os.environ, SDL_VIDEODRIVER='dummy')
        subprocess.run([str(ROOT / 'build/mtg_simulator.exe'), '--export-faces', str(directory), '--profile-file', str(directory / 'export-profile.json')], cwd=ROOT, env=env, check=True)
    entries = re.findall(r'\{"([^"]+)","([^"]+)",([^}]+)\}', (ROOT / 'src/ui/FaceDesigns.hpp').read_text(encoding='utf-8'))
    assert entries
    catalog = []
    for index, (name, description, colors) in enumerate(entries):
        image = f'face-{index:02}.png'
        Image.open(directory / f'face-{index:02}.bmp').convert('RGB').save(directory / image)
        catalog.append(dict(id=index, name=name, description=description, image=image, colors=[f'#{int(value.strip(), 16):06x}' for value in colors.split(',')]))
    (directory / 'catalog.json').write_text(json.dumps(catalog, indent=2), encoding='utf-8')
    if (directory / 'studio.bmp').exists():
        Image.open(directory / 'studio.bmp').convert('RGB').save(directory / 'studio.png')
    rows = (len(entries) + 3) // 4
    height = 100 + rows * 368 + 36
    sheet = Image.new('RGB', (1256, height), '#061018')
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 15), 'ARCANA / WATCH FACES', font=font(31, True), fill='#eef5f6')
    draw.text((26, 60), 'Twelve designs, including five-colour Mana Confluence. Choose in Gear > Watch Face.', font=font(16), fill='#93aab5')
    mask = Image.new('L', (256, 256)); ImageDraw.Draw(mask).ellipse((0, 0, 255, 255), fill=255)
    for index, entry in enumerate(catalog):
        x, y = 24 + (index % 4) * 304, 100 + (index // 4) * 368
        draw.rounded_rectangle((x, y, x + 292, y + 354), 14, fill='#0c1b25', outline='#24404b')
        draw.text((x + 13, y + 14), f'{index:02}', font=font(14), fill='#93aab5')
        draw.text((x + 44, y + 9), entry['name'], font=font(20, True), fill='#eef5f6')
        if index in (0, 10):
            draw.text((x + 241, y + 15), 'KEEP', font=font(11, True), fill='#a7d6b1')
        thumbnail = Image.open(directory / entry['image']).resize((256, 256), Image.Resampling.LANCZOS)
        sheet.paste(thumbnail, (x + 18, y + 43), mask)
        for line, text in enumerate(textwrap.wrap(entry['description'], width=44)[:2]):
            draw.text((x + 16, y + 306 + line * 15), text, font=font(12), fill='#93aab5')
        for swatch, color in enumerate(entry['colors'][:6]):
            draw.rounded_rectangle((x + 16 + swatch * 27, y + 343, x + 36 + swatch * 27, y + 347), 2, fill=color)
    draw.text((26, height - 28), 'Life tap / hold  |  Wheel  |  Opponents  |  Both counters  |  Gear  |  Undo  |  Battery  |  Lethal markers', font=font(13), fill='#93aab5')
    sheet.save(directory / 'overview.png')
    cards = []
    for entry in catalog:
        cards.append(f'<article><div class="title"><small>{entry["id"]:02}</small><h2>{html.escape(entry["name"])}</h2></div><a href="{entry["image"]}"><img src="{entry["image"]}" alt="{html.escape(entry["name"])} face"></a><p>{html.escape(entry["description"])}</p></article>')
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ARCANA Face Studio</title>
<style>body{margin:0;background:#061018;color:#eef5f6;font:16px system-ui}main{max-width:1500px;margin:auto;padding:32px}h1{letter-spacing:.06em;font-size:30px;margin-bottom:8px}.intro,p,small{color:#93aab5}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:18px;margin-top:32px}article{background:#0c1b25;border:1px solid #24404b;border-radius:18px;overflow:hidden}.title{display:flex;gap:14px;align-items:center;padding:0 18px}h2{font-size:18px}img{display:block;width:100%;height:auto;border-radius:50%}p{padding:0 18px 12px;line-height:1.5}a{color:#7ae5ec}</style>
<main><h1>ARCANA / FACE STUDIO</h1><div class="intro">Twelve designs, including Mana Confluence. The original eleven designs remain unchanged. Tap an image for full resolution.</div><p style="padding:0">All concepts keep life tap/hold, rotary edits, opponent tabs, both counter shortcuts, the gear menu, grouped Undo, battery and lethal markers. Use the digital twin's left sidebar or [ / ] to compare without resetting your match. Citrus Relay uses top + and bottom - life regions.</p><a href="overview.png">Open the complete layout overview</a><div class="grid">'''
    (directory / 'index.html').write_text(page + '\n'.join(cards) + '</div></main></html>', encoding='utf-8')
    print(f'Gallery: {directory / "index.html"}\nOverview: {directory / "overview.png"}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'build/face-studio')
    parser.add_argument('--render', action='store_true')
    args = parser.parse_args()
    export(args.directory.resolve(), args.render)
