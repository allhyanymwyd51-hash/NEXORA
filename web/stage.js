(function () {
  'use strict';
  const stage = document.getElementById('game-stage');
  const app = document.querySelector('.app');
  const canvas = document.getElementById('game-canvas');
  const terminal = document.getElementById('stage-terminal');
  const state = document.getElementById('stage-state');
  const keys = new Set();
  let host = null, frameId = null, lastTime = null, source = '', active = false, oldScroll = 0;
  function stop() { if (frameId !== null) cancelAnimationFrame(frameId); frameId = null; lastTime = null; keys.clear(); document.querySelectorAll('[data-key].pressed').forEach(button => button.classList.remove('pressed')); }
  function showError(err) {
    stop(); state.textContent = 'توقف التشغيل'; terminal.hidden = false;
    const message = err instanceof Zayron.ZayronError ? err.message : 'صار خطأ أثناء التشغيل: ' + (err && err.message ? err.message : 'خطأ غير معروف');
    terminal.textContent += (terminal.textContent ? '\n\n' : '') + '⚠ ' + message;
  }
  function start() {
    stop(); terminal.textContent = ''; terminal.hidden = true; state.textContent = 'جاري التشغيل';
    canvas.width = 960; canvas.height = 540;
    try {
      host = ZayronGame.create({context:canvas.getContext('2d'), keys, onOutput(line) { terminal.textContent += (terminal.textContent ? '\n' : '') + line; }});
      const result = host.initialize(source);
      if (!result.graphics && !result.animated) { terminal.hidden = false; if (!terminal.textContent) terminal.textContent = 'تم التشغيل بدون مخرجات.'; state.textContent = 'اكتمل التشغيل'; }
      else { state.textContent = host.animated ? 'اللعبة شغالة' : 'اكتمل الرسم'; }
      if (host.animated) frameId = requestAnimationFrame(tick);
    } catch (err) { showError(err); }
  }
  function tick(now) {
    frameId = null;
    if (!active || !host) return;
    const delta = lastTime === null ? 0 : (now - lastTime) / 1000; lastTime = now;
    try { host.frame(delta); frameId = requestAnimationFrame(tick); } catch (err) { showError(err); }
  }
  function orientation(game) {
    try { if (window.ZayronNative && typeof window.ZayronNative.setGameMode === 'function') window.ZayronNative.setGameMode(game); } catch (_) {}
    if (!game && screen.orientation && screen.orientation.unlock) { try { screen.orientation.unlock(); } catch (_) {} }
  }
  function open(code) { source = code; oldScroll = window.scrollY; active = true; app.hidden = true; stage.hidden = false; window.scrollTo(0,0); orientation(true); start(); }
  function close() { if (!active) return; active = false; stop(); host = null; orientation(false); stage.hidden = true; app.hidden = false; if (document.fullscreenElement && document.exitFullscreen) document.exitFullscreen().catch(()=>{}); window.scrollTo(0,oldScroll); }
  window.closeGameStage = close;
  window.ZayronStage = {open, close};
  document.getElementById('stage-back').addEventListener('click',close);
  document.getElementById('stage-restart').addEventListener('click',start);
  document.getElementById('stage-output').addEventListener('click',()=>{ terminal.hidden = !terminal.hidden; if (!terminal.hidden && !terminal.textContent) terminal.textContent = 'ما فيه مخرجات نصية إلى الآن.'; });
  document.getElementById('stage-fullscreen').addEventListener('click',async()=>{ try { if (stage.requestFullscreen) await stage.requestFullscreen(); if (screen.orientation && screen.orientation.lock) await screen.orientation.lock('landscape'); } catch (_) { state.textContent = 'لف جهازك بالعرض للعب'; } });
  const map = {ArrowUp:'فوق',ArrowDown:'تحت',ArrowLeft:'يسار',ArrowRight:'يمين',w:'فوق',s:'تحت',a:'يسار',d:'يمين'};
  window.addEventListener('keydown',e=>{const key=map[e.key]; if(active && key){keys.add(key);e.preventDefault();}});
  window.addEventListener('keyup',e=>{const key=map[e.key];if(key){keys.delete(key);if(active)e.preventDefault();}});
  window.addEventListener('blur',()=>keys.clear());
  document.addEventListener('visibilitychange',()=>{keys.clear();lastTime=null;});
  document.querySelectorAll('[data-key]').forEach(button=>{
    const key=button.dataset.key;
    button.addEventListener('pointerdown',e=>{e.preventDefault();keys.add(key);button.classList.add('pressed');if(button.setPointerCapture)button.setPointerCapture(e.pointerId);});
    const release=e=>{keys.delete(key);button.classList.remove('pressed');if(e)e.preventDefault();};
    button.addEventListener('pointerup',release);button.addEventListener('pointercancel',release);button.addEventListener('lostpointercapture',release);
  });
})();
