/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
_3v={NULL_POINTER:0};
class _w {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _10 {
	constructor() {}
}
class _11 {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _16 {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _19 {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _1e {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _1h {
	constructor(keyCode) {this.keyCode=keyCode;}
}
class _3o {
	constructor(src) {this.src=src;}
}
class _3y {
	constructor(next, prev, data) {this.next=next;this.prev=prev;this.data=data;}
}
class _42 {
	constructor(head, tail, size) {this.head=head;this.tail=tail;this.size=size;}
}
let _2=0.000000;
let _3=0;
let _4=0;
let _5=new Array(256);
let _6=87;
let _7=65;
let _8=83;
let _9=68;
let _1j;
let _1k;
function _a(){let _l=0;while(_l<256){_5[_l]=false;_l=_l+1;}_1l("canvas");_32("mousemove", _c);_38("keydown", _f);_38("keyup", _i);_3i(_n);}
function _c(_d){}
function _f(_g){_5[_g.keyCode]=true;}
function _i(_j){_5[_j.keyCode]=false;}
function _n(_o){let _q=_o-_2;_2=_o;_1q(255, 255, 255, 255);_2e(0, 0, _3e(), _3g());_1q(255, 255, 128, 0);if(_5[_6]){_4=_4-_q/16.000000;}if(_5[_8]){_4=_4+_q/16.000000;}if(_5[_7]){_3=_3-_q/16.000000;}if(_5[_9]){_3=_3+_q/16.000000;}_2e(_3, _4, 50, 50);_3i(_n);}
function _1l(_1m){_1j=document.getElementById(_1m);_1k=_1j.getContext('2d');}
function _1o(){}
function _1q(_1r, _1s, _1t, _1u){_1k.fillStyle='rgba('+_1s+','+_1t+','+_1u+','+_1r+')';_1k.strokeStyle='rgba('+_1s+','+_1t+','+_1u+','+_1r+')';}
function _1w(_1x){_1k.lineWidth=_1x;}
function _1z(_20){_1k.font=_20;}
function _22(_23, _24, _25, _26){_1k.beginPath();_1k.moveTo(_23,_24);_1k.lineTo(_25,_26);_1k.stroke();}
function _28(_29, _2a, _2b, _2c){_1k.beginPath();_1k.rect(_29,_2a,_2b,_2c);_1k.stroke();}
function _2e(_2f, _2g, _2h, _2i){_1k.fillRect(_2f,_2g,_2h,_2i);}
function _2k(_2l, _2m, _2n){_1k.drawImage(_2l,_2m,_2n);}
function _2p(_2q, _2r, _2s){_1k.fillText(_2q,_2r,_2s);}
function _2u(_2v, _2w){}
function _2y(_2z, _30){}
function _32(_33, _34){_1j.addEventListener(_33,_34);}
function _38(_39, _3a){document.addEventListener(_39,_3a);}
function _3e(){return _1j.width;}
function _3g(){return _1j.height;}
function _3i(_3j){window.requestAnimationFrame(_3j);}
function _3q(_3r){let _3t=new Image();_3t.onload=function(){};_3t.src=_3r;return _3t;}
function _46(){}
function _49(_4a){console.log(_4a);}
function _4c(_4d){}
_a()
