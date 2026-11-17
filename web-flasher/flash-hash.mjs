// js-md5 is CommonJS: browser bundles expose its callable export as default.
export function resolveMd5(module) {
 const md5 = typeof module.default === 'function' ? module.default : module.md5;
 if (typeof md5 !== 'function' || md5(new Uint8Array([97,98,99])) !== '900150983cd24fb0d6963f7d28e17f72') {
  throw new Error('刷写校验组件加载失败，请刷新页面重试（尚未连接或写入设备）');
 }
 return md5;
}
