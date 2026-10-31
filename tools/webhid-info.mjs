export function decodeFirmwareInfo(data) {
  let b=ArrayBuffer.isView(data)?new Uint8Array(data.buffer,data.byteOffset,data.byteLength):new Uint8Array(data);
  if(b.length===25 && b[0]===34)b=b.subarray(1);
  if(b.length!==24 || String.fromCharCode(...b.slice(0,4))!=='XCFW' || b[4]!==1 || b[8]>1 || b[9]&~3 || b[10]>1 || b[11])throw new Error('Identificação de firmware incompatível.');
  const commit=String.fromCharCode(...b.slice(12)).replace(/\0+$/,'');
  if(!/^(?:[0-9a-f]{12}|unknown)$/.test(commit))throw new Error('Commit de firmware inválido.');
  return {version:`${b[5]}.${b[6]}.${b[7]}`,commit,dirty:Boolean(b[10]),role:b[8]?'Receptor':'Guitarra',bluetooth:Boolean(b[9]&1),xsm3:Boolean(b[9]&2)};
}
