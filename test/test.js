/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
_36={NULL_POINTER:0};
class _f {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _j {
	constructor() {}
}
class _k {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _p {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _s {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _x {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _2z {
	constructor(src) {this.src=src;}
}
class _39 {
	constructor(next, prev, data) {this.next=next;this.prev=prev;this.data=data;}
}
class _3d {
	constructor(head, tail, size) {this.head=head;this.tail=tail;this.size=size;}
}
let _2="Hello, world!";
let _3;
let _4=0;
let _5=0;
let _10;
let _11;
function _6(){_3=_31("http://josephs-projects.com/src/image1.jpeg");_12("canvas");_2j("mousemove", _8);_2t(_b);}
function _8(_9){_4=_9.offsetX;_5=_9.offsetY;}
function _b(_c){_21(_3, _4, _5);_2t(_b);}
function _12(_13){_10=document.getElementById(_13);_11=_10.getContext('2d');}
function _15(){}
function _17(_18, _19, _1a, _1b){_11.fillStyle='rgba('+_19+','+_1a+','+_1b+','+_18+')';_11.strokeStyle='rgba('+_19+','+_1a+','+_1b+','+_18+')';}
function _1d(_1e){_11.lineWidth=_1e;}
function _1g(_1h){_11.font=_1h;}
function _1j(_1k, _1l, _1m, _1n){_11.beginPath();_11.moveTo(_1k,_1l);_11.lineTo(_1m,_1n);_11.stroke();}
function _1p(_1q, _1r, _1s, _1t){_11.beginPath();_11.rect(_1q,_1r,_1s,_1t);_11.stroke();}
function _1v(_1w, _1x, _1y, _1z){_11.fillRect(_1w,_1x,_1y,_1z);}
function _21(_22, _23, _24){_11.drawImage(_22,_23,_24);}
function _26(_27, _28, _29){_11.fillText(_27,_28,_29);}
function _2b(_2c, _2d){}
function _2f(_2g, _2h){}
function _2j(_2k, _2l){_10.addEventListener(_2k,_2l);}
function _2p(){return _10.width;}
function _2r(){return _10.height;}
function _2t(_2u){window.requestAnimationFrame(_2u);}
function _31(_32){let _34=new Image();_34.onload=function(){};_34.src=_32;return _34;}
function _3h(){}
function _3k(_3l){console.log(_3l);}
function _3n(_3o){}
_6()
