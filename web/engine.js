/* Zayron experimental interpreter. No eval or network access. */
(function (root) {
  'use strict';
  class ZayronError extends Error {
    constructor(message, line) { super(message + (line ? ` (السطر ${line})` : '')); this.name = 'ZayronError'; }
  }
  const keywords = {
    'خل':'declare', 'يكون':'is', 'يصير':'becomes', 'ورني':'show',
    'لو':'if', 'غير':'other', 'كذا':'thisway', 'دام':'while',
    'لف':'repeat', 'مرات':'times', 'سالفة':'function', 'رجع':'return',
    'صح':'true', 'غلط':'false', 'فاضي':'null', 'و':'and', 'أو':'or', 'او':'or', 'مو':'not',
    'يساوي':'eq', 'أكبر':'gt', 'اكبر':'gt', 'أقل':'lt', 'اقل':'lt', 'من':'than'
  };
  function tokenize(source) {
    const tokens = []; let i = 0, line = 1;
    function add(type, value, at = line) { tokens.push({type, value, line: at}); }
    while (i < source.length) {
      let c = source[i];
      if (c === '\n') { line++; i++; continue; }
      if (/\s/u.test(c)) { i++; continue; }
      if (c === '#' || (c === '/' && source[i + 1] === '/')) { while (i < source.length && source[i] !== '\n') i++; continue; }
      if (c === '"' || c === "'") {
        const quote = c, start = line; let text = '', closed = false; i++;
        while (i < source.length) {
          c = source[i++];
          if (c === quote) { closed = true; break; }
          if (c === '\n') line++;
          if (c === '\\') { if (i >= source.length) break; const next = source[i++]; text += ({n:'\n', t:'\t', r:'\r'}[next] ?? next); }
          else text += c;
        }
        if (!closed) throw new ZayronError('النص مو مقفول بعلامة اقتباس', start);
        add('string', text, start); continue;
      }
      if (/[0-9٠-٩]/u.test(c)) {
        let number = ''; while (i < source.length && /[0-9٠-٩.]/u.test(source[i])) number += source[i++];
        number = number.replace(/[٠-٩]/g, x => String('٠١٢٣٤٥٦٧٨٩'.indexOf(x)));
        if (!/^\d+(\.\d+)?$/.test(number)) throw new ZayronError('العدد مو مكتوب بشكل صحيح', line);
        add('number', Number(number)); continue;
      }
      if (/[\p{L}_]/u.test(c)) {
        let word = ''; while (i < source.length && /[\p{L}\p{M}\p{N}_]/u.test(source[i])) word += source[i++];
        // Optional Arabic vowel marks do not change keywords; identifiers retain their exact spelling.
        const key = word.normalize('NFC').replace(/[\u064b-\u065f\u0670]/g, '');
        add(Object.prototype.hasOwnProperty.call(keywords, key) ? keywords[key] : 'id', word); continue;
      }
      const pair = source.slice(i, i + 2);
      if (['==','!=','<=','>='].includes(pair)) { add(pair, pair); i += 2; continue; }
      if ('+-*/%(){};,<>'.includes(c) || c === '؛' || c === '،') { add(c === '؛' ? ';' : c === '،' ? ',' : c, c); i++; continue; }
      throw new ZayronError(`ما أعرف الرمز «${c}»`, line);
    }
    add('eof', ''); return tokens;
  }
  function parse(source) {
    const tokens = tokenize(source); let i = 0, functionDepth = 0;
    const current = () => tokens[i];
    const match = (...types) => types.includes(current().type) ? tokens[i++] : null;
    function expect(type, message) { const token = match(type); if (!token) throw new ZayronError(message || `المتوقع ${type}`, current().line); return token; }
    const end = () => expect(';', 'حط ؛ بنهاية التعليمة');
    function block() {
      expect('{', 'حط { لبداية المجموعة'); const body = [];
      while (current().type !== '}' && current().type !== 'eof') body.push(statement());
      expect('}', 'المجموعة ما تقفلت، حط }'); return body;
    }
    function statement() {
      const start = current();
      if (match('declare')) { const name = expect('id', 'بعد «خل» اكتب اسم المتغير').value; expect('is', 'بعد اسم المتغير اكتب «يكون»'); const value = expression(); end(); return {kind:'declare', name, value, line:start.line}; }
      if (match('show')) {
        const args = [expression()]; while (match(',')) args.push(expression()); end();
        return {kind:'show', args, line:start.line};
      }
      if (match('if')) {
        const condition = expression(), yes = block(); let no = [];
        if (match('other')) { expect('thisway', 'اكتب «غير كذا» كاملة'); no = block(); }
        return {kind:'if', condition, yes, no, line:start.line};
      }
      if (match('while')) { const condition = expression(); return {kind:'while', condition, body:block(), line:start.line}; }
      if (match('repeat')) { const count = expression(); expect('times', 'بعد عدد اللفات اكتب «مرات»'); return {kind:'repeat', count, body:block(), line:start.line}; }
      if (match('function')) {
        const name = expect('id', 'بعد «سالفة» اكتب اسمها').value; expect('(', 'حط ( بعد اسم السالفة');
        const args = []; if (current().type !== ')') { do { args.push(expect('id', 'اكتب اسم المعامل').value); } while (match(',')); }
        expect(')', 'قفل قائمة المعاملات بـ )'); functionDepth++;
        let body; try { body = block(); } finally { functionDepth--; }
        return {kind:'function', name, args, body, line:start.line};
      }
      if (match('return')) { if (!functionDepth) throw new ZayronError('«رجع» تستخدم داخل سالفة فقط', start.line); const value = expression(); end(); return {kind:'return', value, line:start.line}; }
      if (current().type === 'id' && tokens[i + 1].type === 'becomes') { const name = tokens[i++].value; i++; const value = expression(); end(); return {kind:'assign', name, value, line:start.line}; }
      const value = expression(); end(); return {kind:'expression', value, line:start.line};
    }
    const precedence = {or:1, and:2, eq:3, '==':3, '!=':3, gt:4, lt:4, '<':4, '>':4, '<=':4, '>=':4, '+':5, '-':5, '*':6, '/':6, '%':6};
    function expression(min = 1) {
      const start = current(); let node;
      if (match('number','string')) node = {kind:'literal', value:start.value, line:start.line};
      else if (match('true','false','null')) node = {kind:'literal', value:start.type === 'true' ? true : start.type === 'false' ? false : null, line:start.line};
      else if (match('not','-')) node = {kind:'unary', op:start.type, value:expression(7), line:start.line};
      else if (match('(')) { node = expression(); expect(')', 'قفل القوس بـ )'); }
      else if (match('id')) {
        node = {kind:'variable', name:start.value, line:start.line};
        if (match('(')) { const args = []; if (current().type !== ')') { do { args.push(expression()); } while (match(',')); } expect(')', 'قفل الاستدعاء بـ )'); node = {kind:'call', name:start.value, args, line:start.line}; }
      } else throw new ZayronError('هنا نحتاج قيمة أو تعبير', start.line);
      while (precedence[current().type] >= min) {
        const op = tokens[i++]; if (op.type === 'gt' || op.type === 'lt') expect('than', 'اكتب «من» بعد أكبر أو أقل');
        node = {kind:'binary', op:op.type, left:node, right:expression(precedence[op.type] + 1), line:op.line};
      }
      return node;
    }
    const body = []; while (current().type !== 'eof') body.push(statement()); return body;
  }
  class Env {
    constructor(parent = null) { this.parent = parent; this.vars = new Map(); }
    define(name, value, line) { if (this.vars.has(name)) throw new ZayronError(`الاسم «${name}» مستخدم من قبل`, line); this.vars.set(name, value); }
    get(name, line) { if (this.vars.has(name)) return this.vars.get(name); if (this.parent) return this.parent.get(name, line); throw new ZayronError(`الاسم «${name}» مو معرّف`, line); }
    set(name, value, line) { if (this.vars.has(name)) { this.vars.set(name, value); return; } if (this.parent) return this.parent.set(name, value, line); throw new ZayronError(`المتغير «${name}» مو معرّف`, line); }
  }
  function run(source, options = {}) {
    if (source.length > 100000) throw new ZayronError('الكود أكبر من الحد المسموح');
    const ast = parse(source), output = [], global = new Env(); let steps = 0, depth = 0;
    const maxSteps = options.maxSteps || 30000;
    function tick(line) { if (++steps > maxSteps) throw new ZayronError('وقفنا التنفيذ لأنه تعدّى الحد؛ شيك على التكرار', line); }
    const format = value => value === null ? 'فاضي' : value === true ? 'صح' : value === false ? 'غلط' : String(value);
    if (options.builtins) { for (const [name, fn] of Object.entries(options.builtins)) { if (typeof fn !== 'function') throw new ZayronError('الدالة المدمجة غير صالحة'); global.define(name, fn); } }
    function evaluate(node, env) {
      tick(node.line);
      switch (node.kind) {
        case 'literal': return node.value;
        case 'variable': return env.get(node.name, node.line);
        case 'call': { const fn = env.get(node.name, node.line); if (typeof fn !== 'function') throw new ZayronError(`«${node.name}» مو سالفة`, node.line); return fn(...node.args.map(arg => evaluate(arg, env))); }
        case 'unary': { const value = evaluate(node.value, env); if (node.op === '-') { if (typeof value !== 'number') throw new ZayronError('الإشارة السالبة تحتاج عدد', node.line); return -value; } return !value; }
        case 'binary': {
          const a = evaluate(node.left, env);
          if (node.op === 'and') return Boolean(a) && Boolean(evaluate(node.right, env));
          if (node.op === 'or') return Boolean(a) || Boolean(evaluate(node.right, env));
          const b = evaluate(node.right, env);
          switch (node.op) {
            case '+': if ((typeof a === 'number' && typeof b === 'number') || (typeof a === 'string' && typeof b === 'string')) return a + b; throw new ZayronError('الجمع يحتاج عددين أو نصين', node.line);
            case '-': case '*': case '/': case '%':
              if (typeof a !== 'number' || typeof b !== 'number') throw new ZayronError('الحساب يحتاج أعداد', node.line);
              if ((node.op === '/' || node.op === '%') && b === 0) throw new ZayronError('ما نقدر نقسم على صفر', node.line);
              return node.op === '-' ? a - b : node.op === '*' ? a * b : node.op === '/' ? a / b : a % b;
            case 'eq': case '==': return a === b; case '!=': return a !== b;
            case 'lt': case '<': return a < b; case 'gt': case '>': return a > b; case '<=': return a <= b; case '>=': return a >= b;
          }
        }
      }
      throw new ZayronError('التعبير هذا مو مدعوم', node.line);
    }
    function execute(body, env) {
      for (const statement of body) {
        tick(statement.line); let result;
        switch (statement.kind) {
          case 'declare': env.define(statement.name, evaluate(statement.value, env), statement.line); break;
          case 'assign': env.set(statement.name, evaluate(statement.value, env), statement.line); break;
          case 'show': if (output.length >= 500) throw new ZayronError('وصلنا حد أسطر المخرجات', statement.line); { const line = statement.args.map(arg => format(evaluate(arg, env))).join(' '); output.push(line); if (options.onOutput) options.onOutput(line); } break;
          case 'expression': evaluate(statement.value, env); break;
          case 'if': result = execute(evaluate(statement.condition, env) ? statement.yes : statement.no, new Env(env)); if (result) return result; break;
          case 'while': while (evaluate(statement.condition, env)) { tick(statement.line); result = execute(statement.body, new Env(env)); if (result) return result; } break;
          case 'repeat': {
            const count = evaluate(statement.count, env);
            if (!Number.isSafeInteger(count) || count < 0) throw new ZayronError('عدد اللفات لازم يكون عدداً صحيحاً غير سالب', statement.line);
            for (let round = 1; round <= count; round++) { tick(statement.line); const local = new Env(env); local.define('الجولة', round, statement.line); result = execute(statement.body, local); if (result) return result; }
            break;
          }
          case 'function': {
            const closure = env;
            env.define(statement.name, (...args) => {
              if (args.length !== statement.args.length) throw new ZayronError(`سالفة «${statement.name}» تحتاج ${statement.args.length} معاملات`, statement.line);
              if (++depth > 100) { depth--; throw new ZayronError('تجاوزت حد استدعاء السوالف', statement.line); }
              try { const local = new Env(closure); statement.args.forEach((name, index) => local.define(name, args[index], statement.line)); const r = execute(statement.body, local); return r ? r.value : null; } finally { depth--; }
            }, statement.line); break;
          }
          case 'return': return {value:evaluate(statement.value, env)};
        }
      }
      return null;
    }
    execute(ast, global); return {output, steps, invokeFrame(...calls) { steps = 0; for (const call of calls) { if (call && typeof call.fn === 'function') call.fn(...(call.args || [])); } }};
  }
  const api = {run, parse, ZayronError};
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.Zayron = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
