#!/usr/bin/env node
// Draws the PetiteGUI logo (a pointer over an Ink window holding a yellow folder) and the
// banner (logo and "PetiteGUI" wordmark on Ink's cyan and blue desktop) as PNGs for the
// README, in the EGA colours.
//
//   node tools/logo.js            writes docs/img/logo.png and logo-banner.png
//   node tools/logo.js --c        prints the logo as text, for LOGO[] in src/draw.c
//
// Standard library only. The same picture is drawn in PetiteGUI's About box (src/draw.c),
// there with each pixel two wide to suit the tall pixels.
'use strict';
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const EGA = [
  [0x00, 0x00, 0x00], [0x00, 0x00, 0xAA], [0x00, 0xAA, 0x00], [0x00, 0xAA, 0xAA],
  [0xAA, 0x00, 0x00], [0xAA, 0x00, 0xAA], [0xAA, 0x55, 0x00], [0xAA, 0xAA, 0xAA],
  [0x55, 0x55, 0x55], [0x55, 0x55, 0xFF], [0x55, 0xFF, 0x55], [0x55, 0xFF, 0xFF],
  [0xFF, 0x55, 0x55], [0xFF, 0x55, 0xFF], [0xFF, 0xFF, 0x55], [0xFF, 0xFF, 0xFF],
];
const BLACK = 0, BLUE = 1, CYAN = 3, YELLOW = 14, WHITE = 15;

// The logo as text, one character a pixel. Each letter is a part of the picture, so the
// About box can colour it from the theme: K frame and outlines, S shadow, W window and
// pointer, T title bar, t the marks on it, Y the folder, . nothing.
const LOGO = [
  'KKKKKKKKKKKKKKKKKKK....',
  'KTTTTTTTTTTTTTTTTTKS...',
  'KTttTTTttttttTTTTTKS...',
  'KTTTTTTTTTTTTTTTTTKS...',
  'KKKKKKKKKKKKKKKKKKKS...',
  'KWWWWWWWWWWWWWWWWWKS...',
  'KWWKKKKWWWWWWWWWWWKS...',
  'KWWKYYYKKKKKWWWWWWKS...',
  'KWWKYYYYYYYKWWWWWWKS...',
  'KWWKYYYYYYKKWWWWWWKS...',
  'KWWKYYYYYYKKWWWWWWKS...',
  'KWWKYYYYYYKWKWWWWWKS...',
  'KWWKKKKKKKKWWKWWWWKS...',
  'KWWWWWWWWWKWWWKWWWKS...',
  'KKKKKKKKKKKWWWWKKKKS...',
  '.SSSSSSSSSKWWWWWKSSS...',
  '..........KWWWWWWK.....',
  '..........KWWWWWWWK....',
  '..........KWWWWKKKKK...',
  '..........KWWKWWK......',
  '..........KWK.KWWK.....',
  '..........KK..KWWK.....',
  '..........K....KWWK....',
  '...............KWWK....',
  '................KK.....',
];
// The Ink colours for each part.
const INK = { K: BLACK, S: BLACK, W: WHITE, T: BLUE, t: WHITE, Y: YELLOW };

// The wordmark: a 5x7 bitmap face, two logo pixels to each of its pixels.
const FONT = {
  P: ['XXXX.', 'X...X', 'X...X', 'XXXX.', 'X....', 'X....', 'X....'],
  e: ['.....', '.....', '.XXX.', 'X...X', 'XXXXX', 'X....', '.XXX.'],
  t: ['.X...', '.X...', 'XXXX.', '.X...', '.X...', '.X..X', '..XX.'],
  i: ['..X..', '.....', '.XX..', '..X..', '..X..', '..X..', '.XXX.'],
  G: ['.XXX.', 'X...X', 'X....', 'X.XXX', 'X...X', 'X...X', '.XXX.'],
  U: ['X...X', 'X...X', 'X...X', 'X...X', 'X...X', 'X...X', '.XXX.'],
  I: ['XXX', '.X.', '.X.', '.X.', '.X.', '.X.', 'XXX'],
};

// A picture: w x h colour indexes, -1 transparent.
function picture(w, h) {
  return { w, h, px: new Array(w * h).fill(-1) };
}
function stamp(dst, rows, x0, y0, colourOf, scale = 1) {
  rows.forEach((row, j) => [...row].forEach((ch, i) => {
    const c = colourOf(ch);
    if (c === undefined) return;
    for (let dy = 0; dy < scale; dy++)
      for (let dx = 0; dx < scale; dx++)
        dst.px[(y0 + j * scale + dy) * dst.w + x0 + i * scale + dx] = c;
  }));
}

function logo() {
  const p = picture(LOGO[0].length, LOGO.length);
  stamp(p, LOGO, 0, 0, ch => INK[ch]);
  return p;
}

// Logo, a gap, then "Petite" in ink and "GUI" in accent, centred on the logo.
function lockup(ink, accent) {
  const parts = [['Petite', ink], ['GUI', accent]], gap = 6, fs_ = 2;
  let textW = 0;
  parts.forEach(([s], k) => { for (const ch of s) textW += FONT[ch][0].length + 1; if (k === 0) textW += 2; });
  textW = (textW - 1) * fs_;
  const lg = logo(), w = lg.w + gap + textW, h = lg.h;
  const p = picture(w, h);
  stamp(p, LOGO, 0, 0, ch => INK[ch]);
  let x = lg.w + gap;
  const y = Math.floor((h - 7 * fs_) / 2);
  parts.forEach(([s, c], k) => {
    for (const ch of s) {
      stamp(p, FONT[ch], x, y, v => (v === 'X' ? c : undefined), fs_);
      x += (FONT[ch][0].length + 1) * fs_;
    }
    if (k === 0) x += 2 * fs_;
  });
  return p;
}

// The lockup in the middle of a w x h patch of the Ink desktop: a checker of cyan and blue.
function banner(w, h) {
  const lk = lockup(WHITE, YELLOW), p = picture(w, h);
  const ox = Math.floor((w - lk.w) / 2), oy = Math.floor((h - lk.h) / 2);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const c = lk.px[(y - oy) * lk.w + (x - ox)];
      const inside = x >= ox && x < ox + lk.w && y >= oy && y < oy + lk.h;
      p.px[y * w + x] = inside && c >= 0 ? c : ((x + y) & 1 ? BLUE : CYAN);
    }
  return p;
}

// An RGBA PNG, each picture pixel scale x scale.
function png(p, scale) {
  const W = p.w * scale, H = p.h * scale, raw = Buffer.alloc((W * 4 + 1) * H);
  for (let y = 0; y < H; y++) {
    const row = y * (W * 4 + 1);
    raw[row] = 0;
    for (let x = 0; x < W; x++) {
      const c = p.px[Math.floor(y / scale) * p.w + Math.floor(x / scale)], o = row + 1 + x * 4;
      if (c >= 0) { raw[o] = EGA[c][0]; raw[o + 1] = EGA[c][1]; raw[o + 2] = EGA[c][2]; raw[o + 3] = 255; }
    }
  }
  const crcTable = Array.from({ length: 256 }, (_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  });
  const crc = buf => { let c = 0xFFFFFFFF; for (const b of buf) c = crcTable[(c ^ b) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4), sum = Buffer.alloc(4), body = Buffer.concat([Buffer.from(type), data]);
    len.writeUInt32BE(data.length); sum.writeUInt32BE(crc(body));
    return Buffer.concat([len, body, sum]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4);
  ihdr[8] = 8; ihdr[9] = 6; // 8 bits, RGBA
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0)),
  ]);
}

if (process.argv.includes('--c')) {
  LOGO.forEach(r => console.log(`    "${r}",`));
} else {
  const out = path.join(__dirname, '..', 'docs', 'img');
  fs.writeFileSync(path.join(out, 'logo.png'), png(logo(), 6));
  // Drawn large, as the README stretches it to the page's width.
  fs.writeFileSync(path.join(out, 'logo-banner.png'), png(banner(200, 37), 6));
  console.log('docs/img/logo.png, logo-banner.png');
}
