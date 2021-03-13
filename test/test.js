/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
_3l={NULL_POINTER:0};
class _5 {
	constructor(i) {this.i=i;}
}
class _m {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _q {
	constructor() {}
}
class _r {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _w {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _z {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _14 {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _17 {
	constructor(keycode) {this.keycode=keycode;}
}
class _3e {
	constructor(src) {this.src=src;}
}
class _3o {
	constructor(next, prev, data) {this.next=next;this.prev=prev;this.data=data;}
}
class _3s {
	constructor(head, tail, size) {this.head=head;this.tail=tail;this.size=size;}
}
let _2=0;
let _3=0;
let _4=new Array(256);
let _19;
let _1a;
function _7(){_1b("canvas");_2s("mousemove", _9);_2y("keydown", _c);_2y("keyup", _f);_38(_i);}
function _9(_a){_2=_a.offsetX;_3=_a.offsetY;}
function _c(_d){_4[_d.keycode]=true;_3z(_d);}
function _f(_g){_4[_g.keycode]=false;}
function _i(_j){_38(_i);}
function _1b(_1c){_19=document.getElementById(_1c);_1a=_19.getContext('2d');}
function _1e(){}
function _1g(_1h, _1i, _1j, _1k){_1a.fillStyle='rgba('+_1i+','+_1j+','+_1k+','+_1h+')';_1a.strokeStyle='rgba('+_1i+','+_1j+','+_1k+','+_1h+')';}
function _1m(_1n){_1a.lineWidth=_1n;}
function _1p(_1q){_1a.font=_1q;}
function _1s(_1t, _1u, _1v, _1w){_1a.beginPath();_1a.moveTo(_1t,_1u);_1a.lineTo(_1v,_1w);_1a.stroke();}
function _1y(_1z, _20, _21, _22){_1a.beginPath();_1a.rect(_1z,_20,_21,_22);_1a.stroke();}
function _24(_25, _26, _27, _28){_1a.fillRect(_25,_26,_27,_28);}
function _2a(_2b, _2c, _2d){_1a.drawImage(_2b,_2c,_2d);}
function _2f(_2g, _2h, _2i){_1a.fillText(_2g,_2h,_2i);}
function _2k(_2l, _2m){}
function _2o(_2p, _2q){}
function _2s(_2t, _2u){_19.addEventListener(_2t,_2u);}
function _2y(_2z, _30){document.addEventListener(_2z,_30);}
function _34(){return _19.width;}
function _36(){return _19.height;}
function _38(_39){window.requestAnimationFrame(_39);}
function _3g(_3h){let _3j=new Image();_3j.onload=function(){};_3j.src=_3h;return _3j;}
function _3w(){}
function _3z(_40){console.log(_40);}
function _42(_43){}
_7()
