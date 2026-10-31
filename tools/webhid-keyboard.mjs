export const KEYBOARD_DEFAULTS={keys:[30,31,32,33,34,229,40,41,42,78,0,80,79,228],threshold:8192,hysteresis:2048};
export function validateKeyboard(c){
 return c && Object.keys(c).sort().join()==='hysteresis,keys,threshold' && Array.isArray(c.keys)&&c.keys.length===14&&c.keys.every(k=>Number.isInteger(k)&&(k===0||k>=4&&k<=231))&&Number.isInteger(c.threshold)&&c.threshold>0&&c.threshold<=65535&&Number.isInteger(c.hysteresis)&&c.hysteresis>=0&&c.hysteresis<c.threshold?[]:['Revise as teclas, o limiar e a histerese do whammy.'];
}
export function encodeKeyboard(c){
 const errors=validateKeyboard(c);if(errors.length)throw new Error(errors[0]);
 const p=new Uint8Array(24),v=new DataView(p.buffer);p[0]=1;p.set(c.keys,2);v.setUint16(16,c.threshold,true);v.setUint16(18,c.hysteresis,true);return p;
}
export function decodeKeyboard(data,id=35){
 let p=ArrayBuffer.isView(data)?new Uint8Array(data.buffer,data.byteOffset,data.byteLength):new Uint8Array(data);
 if(p.length===25&&p[0]===id)p=p.subarray(1);
 if(p.length!==24||p[0]!==1||p[1]>3||p.slice(20).some(x=>x))throw new Error('Report de teclado incompatível.');
 const v=new DataView(p.buffer,p.byteOffset,p.byteLength),config={keys:[...p.slice(2,16)],threshold:v.getUint16(16,true),hysteresis:v.getUint16(18,true)};
 const e=validateKeyboard(config);if(e.length)throw new Error(e[0]);return {config,status:p[1]};
}
export function sameKeyboard(a,b){if(validateKeyboard(a).length||validateKeyboard(b).length)return false;return encodeKeyboard(a).every((x,i)=>x===encodeKeyboard(b)[i]);}
export async function readKeyboard(client){return client.exclusive(async()=>decodeKeyboard(await client.transfer(()=>client.device.receiveFeatureReport(35))));}
export async function saveKeyboard(client,c){
 const p=encodeKeyboard(c);return client.exclusive(async()=>{
  await client.transfer(()=>client.device.sendFeatureReport(35,p));const deadline=client.now()+client.timeout;
  do{const r=decodeKeyboard(await client.transfer(()=>client.device.receiveFeatureReport(35)));
   if(r.status>=2)throw new Error(r.status===2?'Teclado rejeitado pelo firmware.':'Falha ao gravar teclado na flash.');
   if(!r.status&&sameKeyboard(r.config,c))return r.config;
   await client.sleep(30);
  }while(client.now()<deadline);throw new Error('Gravação do teclado não confirmada.');
 });
}
