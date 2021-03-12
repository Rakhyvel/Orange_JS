/*
	Generated with Orange compiler
	Written and developed by Joseph Shimel
	https://github.com/rakhyvel/Orange
*/
_9={SPRING:0, WINTER:1, FALL:2, SUMMER:3};
class _e {
	constructor(season) {this.season=season;}
}
class _l {
	constructor(width, height, next) {this.width=width;this.height=height;this.next=next;}
}
class _p {
	constructor() {}
}
class _q {
	constructor(x, y, w, h) {this.x=x;this.y=y;this.w=w;this.h=h;}
}
class _v {
	constructor(x, y) {this.x=x;this.y=y;}
}
class _y {
	constructor(a, r, g, b) {this.a=a;this.r=r;this.g=g;this.b=b;}
}
class _13 {
	constructor(offsetX, offsetY) {this.offsetX=offsetX;this.offsetY=offsetY;}
}
class _3l {
	constructor(src) {this.src=src;}
}
let _2="Hello, world!";
let _3;
let _4=0;
let _5=0;
let _16;
let _17;
function _6(){_3=_3n("http://josephs-projects.com/src/image1.jpeg");_18("canvas");_3e(_g);}
function _g(_h){_2f(_3, _4, _5);_3e(_g);}
function _18(_19){_16=document.getElementById(_19);_17=_16.getContext('2d');}
function _1c(){}
function _1f(_1g, _1h, _1i, _1j){_17.fillStyle='rgba('+_1h+','+_1i+','+_1j+','+_1g+')';_17.strokeStyle='rgba('+_1h+','+_1i+','+_1j+','+_1g+')';}
function _1m(_1n){_17.lineWidth=_1n;}
function _1q(_1r){_17.font=_1r;}
function _1u(_1v, _1w, _1x, _1y){_17.beginPath();_17.moveTo(_1v,_1w);_17.lineTo(_1x,_1y);_17.stroke();}
function _21(_22, _23, _24, _25){_17.beginPath();_17.rect(_22,_23,_24,_25);_17.stroke();}
function _28(_29, _2a, _2b, _2c){_17.fillRect(_29,_2a,_2b,_2c);}
function _2f(_2g, _2h, _2i){_17.drawImage(_2g,_2h,_2i);}
function _2l(_2m, _2n, _2o){_17.fillText(_2m,_2n,_2o);}
function _2r(_2s, _2t){}
function _2w(_2x, _2y){}
function _31(_32, _33){_17.addEventListener(_32,_33);}
function _38(){return _16.width;}
function _3b(){return _16.height;}
function _3e(_3f){window.requestAnimationFrame(_3f);}
function _3n(_3o){let _3r=new Image();_3r.onload=function(){};_3r.src=_3o;return _3r;}
function _3t(_3u){console.log(_3u);}
function _3x(_3y){}
_6()
