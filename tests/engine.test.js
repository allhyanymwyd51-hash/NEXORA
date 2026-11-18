'use strict';
const assert = require('node:assert/strict');
const {run,ZayronError} = require('../web/engine.js');
function output(s){return run(s).output}
assert.deepEqual(output('اطبع("أهلاً بالعالم!")؛'),['أهلاً بالعالم!']);
assert.deepEqual(output('متغير س = ١٠؛ س = س + ٢؛ اطبع(س * ٣)؛'),['36']);
assert.deepEqual(output('let x = 3; if (x > 2) { print("yes"); } else { print("no"); }'),['yes']);
assert.deepEqual(output('متغير ن = ١؛ طالما (ن <= ٣) { اطبع(ن)؛ ن = ن + ١؛ }'),['1','2','3']);
assert.deepEqual(output('دالة ضاعف(ن) { أرجع ن * ٢؛ } اطبع(ضاعف(٥))؛'),['10']);
assert.deepEqual(output('اطبع(صحيح و ليس خطأ، فارغ، "أ" + "ب")؛'),['صحيح فارغ أب']);
assert.deepEqual(output('دالة عاملي(ن) { إذا (ن <= ١) { أرجع ١؛ } أرجع ن * عاملي(ن - ١)؛ } اطبع(عاملي(٥))؛'),['120']);
for (const code of ['اطبع(مجهول)؛','اطبع(٢ / ٠)؛','متغير س = ١؛ متغير س = ٢؛','اطبع("نص)؛','أرجع ١؛']) assert.throws(()=>run(code),ZayronError);
assert.throws(()=>run('طالما (صحيح) { }',{maxSteps:100}),/حد التنفيذ/);
assert.throws(()=>run('دالة لانهائي() { أرجع لانهائي()؛ } لانهائي()؛'),/الحد الأقصى/);
assert.throws(()=>run('اطبع(١، ٢)؛ @'),/السطر/);
console.log('Zayron interpreter tests passed');
