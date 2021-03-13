/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
_3a={NULL_POINTER:0};
class _j {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _n {
	constructor() {}
}
class _o {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _t {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _w {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _11 {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _33 {
	constructor(src) {this.src=src;}
}
class _3d {
	constructor(next, prev, data) {this.next=next;this.prev=prev;this.data=data;}
}
class _3h {
	constructor(head, tail, size) {this.head=head;this.tail=tail;this.size=size;}
}
let _2="Hello, world!";
let _3;
let _4=0;
let _5=0;
let _14;
let _15;
function _6(){_3=_35("http://josephs-projects.com/src/image1.jpeg");_16("canvas");_2n("mousemove", _8);_2x(_b);}
function _8(_9){_4=_9.offsetX;_5=_9.offsetY;}
function _b(_c){_25(_3, _4, _5);_2x(_b);let _e;}
function _f(){let _h;}
function _16(_17){_14=document.getElementById(_17);_15=_14.getContext('2d');}
function _19(){}
function _1b(_1c, _1d, _1e, _1f){_15.fillStyle='rgba('+_1d+','+_1e+','+_1f+','+_1c+')';_15.strokeStyle='rgba('+_1d+','+_1e+','+_1f+','+_1c+')';}
function _1h(_1i){_15.lineWidth=_1i;}
function _1k(_1l){_15.font=_1l;}
function _1n(_1o, _1p, _1q, _1r){_15.beginPath();_15.moveTo(_1o,_1p);_15.lineTo(_1q,_1r);_15.stroke();}
function _1t(_1u, _1v, _1w, _1x){_15.beginPath();_15.rect(_1u,_1v,_1w,_1x);_15.stroke();}
function _1z(_20, _21, _22, _23){_15.fillRect(_20,_21,_22,_23);}
function _25(_26, _27, _28){_15.drawImage(_26,_27,_28);}
function _2a(_2b, _2c, _2d){_15.fillText(_2b,_2c,_2d);}
function _2f(_2g, _2h){}
function _2j(_2k, _2l){}
function _2n(_2o, _2p){_14.addEventListener(_2o,_2p);}
function _2t(){return _14.width;}
function _2v(){return _14.height;}
function _2x(_2y){window.requestAnimationFrame(_2y);}
function _35(_36){let _38=new Image();_38.onload=function(){};_38.src=_36;return _38;}
function _3l(){}
function _3o(_3p){console.log(_3p);}
function _3r(_3s){}
_6()
