import test from 'node:test';
import assert from 'node:assert/strict';
import {decodeFirmwareInfo} from '../tools/webhid-info.mjs';
test('firmware version, role, capabilities, dirty commit and report ID',()=>{
 const p=new Uint8Array(24);p.set([88,67,70,87,1,0,1,0,1,3,1]);p.set(new TextEncoder().encode('123456abcdef'),12);
 const info=decodeFirmwareInfo(p);assert.equal(info.version,'0.1.0');assert.equal(info.role,'Receptor');assert.equal(info.bluetooth,true);assert.equal(info.dirty,true);
 assert.deepEqual(decodeFirmwareInfo(Uint8Array.from([34,...p])),info);
 p[4]=2;assert.throws(()=>decodeFirmwareInfo(p));p[4]=1;p[9]=4;assert.throws(()=>decodeFirmwareInfo(p));
 assert.throws(()=>decodeFirmwareInfo(new Uint8Array(12)));
});
