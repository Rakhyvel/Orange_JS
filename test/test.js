/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
class _6 {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _a {
	constructor() {}
}
class _b {
	constructor() {}
}
class _c {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _h {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _k {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _p {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
let _s;
let _t;
function _2(){_31("Hello, world!");}
function _u(_v){_s=document.getElementByID(_v);_t=_s.getContext('2d');}
function _y(){}
function _11(_12, _13, _14, _15){_t.fillStyle=rgba(_13,_14,_15,);_t.strokeStyle=rgba(_13,_14,_15,);}
function _18(_19){_t.lineWidth=_19;}
function _1c(_1d){_t.font=_1d;}
function _1g(_1h, _1i, _1j, _1k){_t.beginPath();_t.moveTo(_1h,_1i);_t.lineTo(_1j,_1k);_t.stroke();}
function _1n(_1o, _1p, _1q, _1r){_t.beginPath();_t.rect(_1o,_1p,_1q,_1r);_t.stroke();}
function _1u(_1v, _1w, _1x, _1y){_t.fillRect(_1v,_1w,_1x,_1y);}
function _21(_22, _23, _24){}
function _27(_28, _29, _2a){_t.fillText(_28,_29,_2a);}
function _2d(_2e, _2f){}
function _2i(_2j, _2k){}
function _2n(_2o, _2p){_t.addEventListener(_2o,_2p);}
function _2u(){return _s.width;}
function _2x(){return _s.height;}
function _31(_32){console.log(_32);}
function _35(_36){}
_2()
