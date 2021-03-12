/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
class _e {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _i {
	constructor() {}
}
class _j {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _o {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _r {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _w {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _3e {
	constructor(src) {this.src=src;}
}
let _2="Hello, world!";
let _3;
let _4=0;
let _5=0;
let _z;
let _10;
function _6(){_3=_3g("http://josephs-projects.com/src/image1.jpeg");_11("canvas");_37(_9);}
function _9(_a){_3m("Hello, world!");_28(_3, _4, _5);_37(_9);}
function _11(_12){_z=document.getElementById(_12);_10=_z.getContext('2d');}
function _15(){}
function _18(_19, _1a, _1b, _1c){_10.fillStyle='rgba('+_1a+','+_1b+','+_1c+','+_19+')';_10.strokeStyle='rgba('+_1a+','+_1b+','+_1c+','+_19+')';}
function _1f(_1g){_10.lineWidth=_1g;}
function _1j(_1k){_10.font=_1k;}
function _1n(_1o, _1p, _1q, _1r){_10.beginPath();_10.moveTo(_1o,_1p);_10.lineTo(_1q,_1r);_10.stroke();}
function _1u(_1v, _1w, _1x, _1y){_10.beginPath();_10.rect(_1v,_1w,_1x,_1y);_10.stroke();}
function _21(_22, _23, _24, _25){_10.fillRect(_22,_23,_24,_25);}
function _28(_29, _2a, _2b){_10.drawImage(_29,_2a,_2b);}
function _2e(_2f, _2g, _2h){_10.fillText(_2f,_2g,_2h);}
function _2k(_2l, _2m){}
function _2p(_2q, _2r){}
function _2u(_2v, _2w){_10.addEventListener(_2v,_2w);}
function _31(){return _z.width;}
function _34(){return _z.height;}
function _37(_38){window.requestAnimationFrame(_38);}
function _3g(_3h){let _3k=new Image();_3k.onload=function(){};_3k.src=_3h;return _3k;}
function _3m(_3n){console.log(_3n);}
function _3q(_3r){}
_6()
