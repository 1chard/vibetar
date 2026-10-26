import test from 'node:test';
import assert from 'node:assert/strict';
import { EXTRA_DEFAULTS, encodeExtras, decodeExtras, validateExtras, readExtras, saveExtras, pickupIndex } from '../tools/webhid-extras.mjs';
const base = { buttons: Object.fromEntries(Array.from({length:11}, (_,i) => [String(i), i+2])), mode_gpio:13, whammy:{gpio:26,enabled:true} };
test('five ADC references roundtrip with closest position, boundaries, inversion and arbitrary order',()=>{
  const e=structuredClone(EXTRA_DEFAULTS);Object.assign(e.pickup,{gpio:27,values:[90,340,1200,2900,3950]});
  const bytes=encodeExtras(e);assert.equal(bytes[0],2);assert.equal(bytes.length,24);
  assert.deepEqual(decodeExtras(bytes).extras,e);
  assert.equal(pickupIndex(500,e.pickup),1);assert.equal(pickupIndex(770,e.pickup),1);
  assert.equal(pickupIndex(771,e.pickup),2);assert.equal(pickupIndex(0,e.pickup),0);
  assert.equal(pickupIndex(4095,e.pickup),4);e.pickup.inverted=true;assert.equal(pickupIndex(4095,e.pickup),0);
  e.pickup.values.reverse();e.pickup.inverted=false;assert.equal(pickupIndex(100,e.pickup),4);
  bytes[22]=1;assert.throws(()=>decodeExtras(bytes));
  for(const values of [[1,1,2,3,4],[1,2,3,4],[1,2,3,4,4096],[1,2,3,4,NaN]]){
    e.pickup.values=values;assert.ok(validateExtras(e).length);assert.throws(()=>encodeExtras(e));
  }
});
test('save confirms nonlinear references in flash',async()=>{
  const e=structuredClone(EXTRA_DEFAULTS);e.pickup.values=[3950,2900,1200,340,90];
  assert.deepEqual(await saveExtras(mock(),e,base),e);
});
test('optional report roundtrip, report ID and offset; inactive by default', () => {
  const e = structuredClone(EXTRA_DEFAULTS); e.pins.left=0; e.pins.solo_modifier=14; e.pickup={gpio:27,inverted:true,min:125,max:3900};
  const payload=encodeExtras(e,base); assert.equal(payload.length,24); assert.equal(payload[2],0); assert.equal(payload[11],1);
  const os=new Uint8Array(28); os[2]=7; os.set(payload,3);
  assert.deepEqual(decodeExtras(new DataView(os.buffer,2,25)).extras,e);
  assert.deepEqual(decodeExtras(encodeExtras(EXTRA_DEFAULTS)).extras,EXTRA_DEFAULTS);
});
test('reject pin conflicts, internal GPIO, ADC collisions, calibration and malformed bytes', () => {
  let e=structuredClone(EXTRA_DEFAULTS); e.pins.left=2; assert.ok(validateExtras(e,base).length);
  e.pins.left=0; e.pins.right=0; assert.ok(validateExtras(e,base).length);
  e.pins.right=23; assert.ok(validateExtras(e,base).length);
  e.pins.right=255; e.pickup.gpio=26; assert.ok(validateExtras(e,base).length);
  e.pickup.gpio=27; e.pickup.min=e.pickup.max; assert.ok(validateExtras(e,base).length);
  const bytes=encodeExtras(EXTRA_DEFAULTS); bytes[16]=1; assert.throws(()=>decodeExtras(bytes));
  bytes[16]=0; bytes[11]=2; assert.throws(()=>decodeExtras(bytes));
  assert.throws(()=>decodeExtras(bytes.subarray(0,23)));
});
function mock({status=0,stale=false}={}) {
  let stored=encodeExtras(EXTRA_DEFAULTS), reads=0;
  const device={ async sendFeatureReport(id,p){assert.equal(id,7); if(!stale) stored=p.slice();},
    async receiveFeatureReport(id){assert.equal(id,7); const b=stored.slice(); b[1]=reads++===0?1:status; return new DataView(b.buffer);} };
  let now=0;
  return {device,exclusive:fn=>fn(),transfer:fn=>fn(),now:()=>now,timeout:100,sleep:async ms=>{now+=ms;}};
}
test('save confirms flash status AND values after Pending', async () => {
  const client=mock(), e=structuredClone(EXTRA_DEFAULTS); e.pins.left=0;
  assert.deepEqual(await saveExtras(client,e,base),e);
  assert.deepEqual((await readExtras(client)).extras,e);
});
test('flash errors, rejected config and stale Ready cannot announce success', async () => {
  const e=structuredClone(EXTRA_DEFAULTS); e.pins.right=1;
  await assert.rejects(saveExtras(mock({status:3}),e,base),/flash/);
  await assert.rejects(saveExtras(mock({status:2}),e,base),/rejeitada/);
  await assert.rejects(saveExtras(mock({stale:true}),e,base),/não confirmada/);
});
