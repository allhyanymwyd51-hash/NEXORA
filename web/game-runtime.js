/* Minimal 2D host for the Zayron interpreter. Canvas dimensions are logical pixels. */
(function (root) {
  'use strict';
  const WIDTH = 960, HEIGHT = 540;
  function create(options) {
    const ctx = options.context;
    if (!ctx) throw new Error('المتصفح ما يدعم الرسم ثنائي الأبعاد.');
    let update = null, draw = null, background = '#111326', hasGraphics = false, runtime;
    const keys = options.keys || new Set();
    const error = message => { throw new root.Zayron.ZayronError(message); };
    const number = (v, name) => { if (typeof v !== 'number' || !Number.isFinite(v) || Math.abs(v) > 1000000) error(`${name} يحتاج عدداً مناسباً`); return v; };
    const color = v => { if (typeof v !== 'string' || !/^#[0-9a-fA-F]{6}$/.test(v)) error('اللون لازم يكون نصاً مثل #aabbcc'); return v; };
    const fn = (name, count, cb) => (...args) => { if (args.length !== count) error(`«${name}» تحتاج ${count} معاملات`); return cb(...args); };
    const builtins = {
      'خلفية': fn('خلفية', 1, c => { background = color(c); ctx.fillStyle = background; ctx.fillRect(0,0,WIDTH,HEIGHT); hasGraphics = true; return null; }),
      'مستطيل': fn('مستطيل', 5, (x,y,w,h,c) => { [x,y,w,h].forEach(v => number(v, 'المستطيل')); if (w < 0 || h < 0) error('عرض وارتفاع المستطيل لازم يكونان غير سالبين'); ctx.fillStyle = color(c); ctx.fillRect(x,y,w,h); hasGraphics = true; return null; }),
      'دائرة': fn('دائرة', 4, (x,y,r,c) => { [x,y,r].forEach(v => number(v, 'الدائرة')); if (r < 0) error('نصف قطر الدائرة لازم يكون غير سالب'); ctx.fillStyle = color(c); ctx.beginPath(); ctx.arc(x,y,r,0,Math.PI*2); ctx.fill(); hasGraphics = true; return null; }),
      'اكتب_على_الشاشة': fn('اكتب_على_الشاشة', 5, (text,x,y,size,c) => { [x,y,size].forEach(v => number(v, 'النص')); if (size < 1 || size > 200) error('حجم النص لازم يكون بين ١ و٢٠٠'); ctx.fillStyle = color(c); ctx.font = `bold ${size}px sans-serif`; ctx.textAlign = 'right'; ctx.direction = 'rtl'; ctx.fillText(String(text).slice(0, 500),x,y); hasGraphics = true; return null; }),
      'مضغوط': fn('مضغوط', 1, key => { if (typeof key !== 'string') error('اسم الزر لازم يكون نصاً'); return keys.has(key); }),
      'يتلامس': fn('يتلامس', 8, (x,y,w,h,a,b,c,d) => { [x,y,w,h,a,b,c,d].forEach(v => number(v, 'التلامس')); if (w < 0 || h < 0 || c < 0 || d < 0) error('أبعاد التلامس لازم تكون غير سالبة'); return x < a+c && x+w > a && y < b+d && y+h > b; }),
      'عشوائي': fn('عشوائي', 2, (min,max) => { number(min, 'عشوائي'); number(max, 'عشوائي'); if (!Number.isInteger(min) || !Number.isInteger(max) || min > max) error('عشوائي تحتاج حدين صحيحين مرتبين'); return Math.floor((options.random || Math.random)() * (max-min+1)) + min; }),
      'وقت_التحديث': fn('وقت_التحديث', 1, cb => { if (typeof cb !== 'function') error('وقت_التحديث تحتاج اسم سالفة'); update = cb; return null; }),
      'وقت_الرسم': fn('وقت_الرسم', 1, cb => { if (typeof cb !== 'function') error('وقت_الرسم تحتاج اسم سالفة'); draw = cb; hasGraphics = true; return null; })
    };
    function initialize(source) { runtime = root.Zayron.run(source, {builtins, onOutput:options.onOutput}); return {output:runtime.output, graphics:hasGraphics, animated:!!(update || draw)}; }
    function frame(delta) {
      if (!runtime) throw new Error('المشروع ما بدأ');
      const calls = []; if (update) calls.push({fn:update,args:[Math.max(0,Math.min(number(delta,'الوقت'),0.05))]});
      // Clear immediately before drawing, after the update callback has run.
      if (draw) calls.push({fn:() => { ctx.fillStyle = background; ctx.fillRect(0,0,WIDTH,HEIGHT); draw(); }});
      runtime.invokeFrame(...calls);
    }
    return {initialize, frame, get animated() { return !!(update || draw); }, get graphics() { return hasGraphics; }};
  }
  const api = {create, WIDTH, HEIGHT};
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.ZayronGame = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
