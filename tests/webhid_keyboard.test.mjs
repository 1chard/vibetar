import test from 'node:test';
import assert from 'node:assert/strict';
import {KEYBOARD_DEFAULTS,encodeKeyboard,decodeKeyboard,validateKeyboard,saveKeyboard} from '../tools/webhid-keyboard.mjs';
test('Festival defaults and rebind wire roundtrip, offsets and malformed reports',()=>{
 const c=structuredClone(KEYBOARD_DEFAULTS);assert.deepEqual(c.keys.slice(0,7),[30,31,32,33,34,229,40]);
 c.keys[0]=4;assert.deepEqual(decodeKeyboard(encodeKeyboard(c)).config,c);
 assert.deepEqual(decodeKeyboard(Uint8Array.from([35,...encodeKeyboard(c)])).config,c);
 for(const key of [1,232,-1,1.5]){c.keys[0]=key;assert.ok(validateKeyboard(c).length);}
 const bad=encodeKeyboard(KEYBOARD_DEFAULTS);bad[23]=1;assert.throws(()=>decodeKeyboard(bad));
 assert.throws(()=>decodeKeyboard(bad.slice(1)));c.keys[0]=0;c.threshold=c.hysteresis;assert.ok(validateKeyboard(c).length);
});
test('save confirms values and rejects flash error or stale ready',async()=>{
 for(const mode of ['ok','stale','error']){
  let stored=encodeKeyboard(KEYBOARD_DEFAULTS),now=0;
  const c=structuredClone(KEYBOARD_DEFAULTS);c.keys[0]=4;
  const client={exclusive:f=>f(),transfer:f=>f(),now:()=>now,timeout:100,sleep:async ms=>{now+=ms;},device:{sendFeatureReport:async(id,p)=>{assert.equal(id,35);if(mode!=='stale')stored=p.slice();},receiveFeatureReport:async()=>{const p=stored.slice();if(mode==='error')p[1]=3;return p;}}};
  if(mode==='ok')assert.deepEqual(await saveKeyboard(client,c),c);else await assert.rejects(saveKeyboard(client,c));
 }
});
