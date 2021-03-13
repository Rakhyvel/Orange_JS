/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
_38={NULL_POINTER:0};
class _h {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _l {
	constructor() {}
}
class _m {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _r {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _u {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _z {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _31 {
	constructor(src) {this.src=src;}
}
class _3b {
	constructor(next, prev, data) {this.next=next;this.prev=prev;this.data=data;}
}
class _3f {
	constructor(head, tail, size) {this.head=head;this.tail=tail;this.size=size;}
}
let _2="Hello, world!";
let _3;
let _4=0;
let _5=0;
let _12;
let _13;
function _6(){_3=_33("http://josephs-projects.com/src/image1.jpeg");_14("canvas");_2l("mousemove", _8);_2v(_b);}
function _8(_9){_4=_9.offsetX;_5=_9.offsetY;}
function _b(_c){_23(_3, _4, _5);_2v(_b);let _e=new Array(4);let _f;}
function _14(_15){_12=document.getElementById(_15);_13=_12.getContext('2d');}
function _17(){}
function _19(_1a, _1b, _1c, _1d){_13.fillStyle='rgba('+_1b+','+_1c+','+_1d+','+_1a+')';_13.strokeStyle='rgba('+_1b+','+_1c+','+_1d+','+_1a+')';}
function _1f(_1g){_13.lineWidth=_1g;}
function _1i(_1j){_13.font=_1j;}
function _1l(_1m, _1n, _1o, _1p){_13.beginPath();_13.moveTo(_1m,_1n);_13.lineTo(_1o,_1p);_13.stroke();}
function _1r(_1s, _1t, _1u, _1v){_13.beginPath();_13.rect(_1s,_1t,_1u,_1v);_13.stroke();}
function _1x(_1y, _1z, _20, _21){_13.fillRect(_1y,_1z,_20,_21);}
function _23(_24, _25, _26){_13.drawImage(_24,_25,_26);}
function _28(_29, _2a, _2b){_13.fillText(_29,_2a,_2b);}
function _2d(_2e, _2f){}
function _2h(_2i, _2j){}
function _2l(_2m, _2n){_12.addEventListener(_2m,_2n);}
function _2r(){return _12.width;}
function _2t(){return _12.height;}
function _2v(_2w){window.requestAnimationFrame(_2w);}
function _33(_34){let _36=new Image();_36.onload=function(){};_36.src=_34;return _36;}
function _3j(){}
function _3m(_3n){console.log(_3n);}
function _3p(_3q){}
_6()
