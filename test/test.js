/*** BEGIN STRUCTS ****/
class Any {
	constructor() {
	}
}
class None {
	constructor() {
	}
}
class Canvas {
	constructor() {
	}
}
class Image {
	constructor() {
	}
}
class Rect {
	constructor(x, y, w, h) {
		this.x=x;
		this.y=y;
		this.w=w;
		this.h=h;
	}
}
class Point {
	constructor(x, y) {
		this.x=x;
		this.y=y;
	}
}
class Color {
	constructor(a, r, g, b) {
		this.a=a;
		this.r=r;
		this.g=g;
		this.b=b;
	}
}
class Event {
	constructor(type, key, mouse, wheel, moved) {
		this.type=type;
		this.key=key;
		this.mouse=mouse;
		this.wheel=wheel;
		this.moved=moved;
	}
}

/*** BEGIN SYSTEM/CANVAS ****/
function System_println(msg){
	console.log(msg);
}
function System_input(msg){
	return window.prompt(msg);
}let canvas;let context;function Canvas_init(id){canvas=document.getElementById(id);context=canvas.getContext('2d');}function Canvas_setColor(color){context.fillStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setStroke(w, color){context.width=w;context.strokeStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setFont(font){context.font=font;}function Canvas_drawLine(x1, y1, x2, y2){context.beginPath();context.moveTo(x1, y1);context.lineTo(x2, y2);context.stroke();}function Canvas_drawRect(x,y,w,h){context.beginPath();context.rect(x,y,w,h);context.stroke();}function Canvas_fillRect(x,y,w,h){context.fillRect(x,y,w,h);}function Canvas_drawString(text, x, y){context.fillText(text, x, y);}
/*** BEGIN GLOBALS ****/

/*** BEGIN FUNCTIONS ****/
function Main_start(){
	Canvas_init("canvas");
	Canvas_setColor(new Color(255, 255, 128, 0));
	let i=0;
	while(i<5){
		Canvas_fillRect(i*10, i*10, 10, 10);
		i=i+1;
	}
	Main_otherFunc();
}
function Main_otherFunc(){
	System_println("Hello, world!");
}
Main_start();